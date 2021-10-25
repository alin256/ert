import io
import os
import sys
import threading
import json
import time

from select import select, PIPE_BUF
from subprocess import Popen, TimeoutExpired
from pathlib import Path
from time import sleep
from logging import getLogger
from typing import (
    Any,
    Callable,
    Generic,
    List,
    Mapping,
    Optional,
    Sequence,
    Type,
    TypeVar,
    Union,
    TYPE_CHECKING,
)

if TYPE_CHECKING:
    from inspect import Traceback

logger = getLogger("ert_shared.storage")
T = TypeVar("T", bound="BaseService")
ConnInfo = Union[Mapping[str, Any], Exception, None]


def local_exec_args(script_args: Union[str, List[str]]) -> List[str]:
    """
    Convenience function that returns the exec_args for executing a Python
    script in the directory of '_base_service.py'.

    This is done instead of using 'python -m [module path]' due to the '-m' flag
    adding the user's current working directory to sys.path. Executing a Python
    script by itself will add the directory of the script rather than the
    current working directory, thus we avoid accidentally importing user's
    directories that just happen to have the same names as the ones we use.
    """
    if isinstance(script_args, str):
        script = script_args
        rest: List[str] = []
    else:
        script = script_args[0]
        rest = script_args[1:]
    script = f"_{script}_main.py"
    return [sys.executable, str(Path(__file__).parent / script), *rest]


class _Context(Generic[T]):
    def __init__(self, service: T) -> None:
        self._service = service

    def __enter__(self) -> T:
        return self._service

    def __exit__(
        self,
        exc_type: Type[BaseException],
        exc_value: BaseException,
        traceback: "Traceback",
    ) -> bool:
        self._service.shutdown()
        return exc_type is None


class _Proc(threading.Thread):
    def __init__(
        self,
        service_name: str,
        exec_args: Sequence[str],
        timeout: int,
        set_conn_info: Callable[[ConnInfo], None],
    ):
        super().__init__()

        self._shutdown = threading.Event()

        self._service_name = service_name
        self._exec_args = exec_args
        self._timeout = timeout
        self._set_conn_info = set_conn_info

        self._assert_server_not_running()

        fd_read, fd_write = os.pipe()
        self._comm_pipe = os.fdopen(fd_read)

        env = os.environ.copy()
        env["ERT_COMM_FD"] = str(fd_write)
        self._proc = Popen(
            self._exec_args,
            pass_fds=(fd_write,),
            env=env,
            close_fds=True,
        )
        os.close(fd_write)

    def run(self) -> None:
        comm = self._read_conn_info(self._proc)

        if comm is None:
            self._set_conn_info(TimeoutError())
            return  # _read_conn_info() has already cleaned up in this case

        conn_info: ConnInfo = None
        try:
            conn_info = json.loads(comm)
        except json.JSONDecodeError:
            conn_info = ServerBootFail()
        except Exception as exc:
            conn_info = exc

        self._set_conn_info(conn_info)

        while True:
            if self._proc.poll() is not None:
                break
            if self._shutdown.wait(1):
                self._do_shutdown()
                break

        self._ensure_delete_conn_info()

    def shutdown(self) -> int:
        """Shutdown the server."""
        self._shutdown.set()
        self.join()
        return self._proc.returncode

    def _assert_server_not_running(self) -> None:
        """It doesn't seem to be possible to check whether a server has been started
        other than looking for files that were created during the startup process.
        Due to possible race-condition we do a retry if file is present to make sure
        we have waited long enough before sys.exit
        """
        for i in range(3):
            if (Path.cwd() / f"{self._service_name}_server.json").exists():
                print(
                    f"{self._service_name}_server.json is present on this location. Retry {i}"
                )
                time.sleep(1)

        if (Path.cwd() / f"{self._service_name}_server.json").exists():
            print(
                f"A file called {self._service_name}_server.json is present from this location. "
                "This indicates there is already a ert instance running. If you are "
                "certain that is not the case, try to delete the file and try "
                "again."
            )
            sys.exit(1)

    def _read_conn_info(self, proc: Popen) -> Optional[str]:
        comm_buf = io.StringIO()
        first_iter = True
        while first_iter or proc.poll() is None:
            first_iter = False
            ready = select([self._comm_pipe], [], [], self._timeout)

            # Timeout reached, exit with a failure
            if ready == ([], [], []):
                self._do_shutdown()
                self._ensure_delete_conn_info()
                return None

            x = self._comm_pipe.read(PIPE_BUF)
            if x == "":  # EOF
                break
            comm_buf.write(x)
        return comm_buf.getvalue()

    def _do_shutdown(self) -> None:
        if self._proc is None:
            return
        try:
            self._proc.terminate()
            self._proc.wait(10)
        except TimeoutExpired:
            self._proc.kill()
            self._proc.wait()

    def _ensure_delete_conn_info(self) -> None:
        """
        Ensure that the JSON connection information file is deleted
        """
        file = Path(f"{self._service_name}_server.json")
        if file.exists():
            file.unlink()


class ServerBootFail(RuntimeError):
    pass


class BaseService:
    """
    BaseService provides a block-only-when-needed mechanism for starting and
    maintaining services as subprocesses.

    This is achieved by using a POSIX communication pipe, over which the service
    can communicate that it has started. The contents of the communication is
    also written to a file inside of the ERT storage directory.

    The service itself can implement the other side of the pipe as such::

        import os

        # ... perform initialisation ...

        # BaseService provides this environment variable with the pipe's FD
        comm_fd = os.environ["ERT_COMM_FD"]

        # Open the pipe with Python's IO classes for ease of use
        with os.fdopen(comm_fd, "wb") as comm:
            # Write JSON over the pipe, which will be interpreted by a subclass
            # of BaseService on ERT's side
            comm.write('{"some": "json"}')

        # The pipe is flushed and closed here. This tells BaseService that
        # initialisation is finished and it will try to read the JSON data.
    """

    _instance: Optional["BaseService"] = None

    def __init__(
        self,
        exec_args: Sequence[str],
        timeout: int = 20,
        conn_info: ConnInfo = None,
    ):
        self._exec_args = exec_args
        self._timeout = timeout

        self._proc: Optional[_Proc] = None
        self._conn_info: ConnInfo = conn_info
        self._conn_info_event = threading.Event()

        # Flag that we have connection information
        if self._conn_info:
            self._conn_info_event.set()
        else:
            self._proc = _Proc(
                self.service_name,
                exec_args,
                timeout,
                self.set_conn_info,
            )

    @classmethod
    def start_server(cls: Type[T], *args, **kwargs) -> _Context[T]:
        if cls._instance is not None:
            raise RuntimeError("Storage server already running")
        cls._instance = obj = cls(*args, **kwargs)
        if obj._proc is not None:
            obj._proc.start()
        return _Context(obj)

    @classmethod
    def connect(
        cls: Type[T],
        *,
        project: Optional[os.PathLike] = None,
        timeout: Optional[int] = None,
    ) -> T:
        if cls._instance is not None:
            cls._instance.wait_until_ready()
            assert isinstance(cls._instance, cls)
            return cls._instance

        path = Path(project) if project is not None else Path.cwd()
        name = f"{cls.service_name}_server.json"

        if timeout is None:
            timeout = 20
        t = -1
        while t < timeout:
            search_path = Path(path)
            while search_path != Path("/"):
                if (search_path / name).exists():
                    with (search_path / name).open() as f:
                        return cls([], conn_info=json.load(f))
                search_path = search_path.parent

            sleep(1)
            t += 1

        raise TimeoutError("Server not started")

    @classmethod
    def connect_or_start_server(cls: Type[T], *args, **kwargs) -> _Context[T]:
        try:
            return _Context(cls.connect(timeout=0))
        except TimeoutError:
            # Server is not running. Start a new one
            pass
        return cls.start_server(*args, **kwargs)

    def wait_until_ready(self, timeout: Optional[float] = None) -> bool:
        self._conn_info_event.wait(timeout)
        if self._conn_info_event.is_set():
            return not (
                self._conn_info is None or isinstance(self._conn_info, Exception)
            )
        return False  # Timeout reached

    def wait(self) -> None:
        if self._proc is not None:
            self._proc.join()

    def set_conn_info(self, info: ConnInfo) -> None:
        """ """
        if self._conn_info is not None:
            raise ValueError("Connection information already set")
        if info is None:
            raise ValueError
        self._conn_info = info

        if isinstance(info, Mapping):
            with open(f"{self.service_name}_server.json", "w") as f:
                json.dump(info, f)

        self._conn_info_event.set()

    def fetch_conn_info(self, timeout: Optional[float] = None) -> Mapping[str, Any]:
        self.wait_until_ready(timeout)
        if self._conn_info is None:
            raise ValueError("conn_info is None")
        elif isinstance(self._conn_info, Exception):
            raise self._conn_info
        return self._conn_info

    def shutdown(self) -> int:
        """Shutdown the server."""
        if self._proc is None:
            return -1
        self.__class__._instance = None
        proc, self._proc = self._proc, None
        return proc.shutdown()

    @property
    def service_name(self) -> str:
        """
        Subclass should return the name of the service, eg 'storage' for ERT Storage.
        Used for identifying the server information JSON file.
        """
        raise NotImplementedError

    @property
    def _service_file(self) -> str:
        return f"{self.service_name}_server.json"