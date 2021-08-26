import contextlib
import json
import pathlib
import pickle
import tempfile

from typing import Callable, ContextManager

import cloudpickle
import pytest
from ert_utils import tmp

from ert.data import (
    InMemoryRecordTransmitter,
    RecordTransmitter,
    SharedDiskRecordTransmitter,
)
import ert


@pytest.fixture()
def storage_path(workspace, ert_storage):
    yield ert.storage.get_records_url(workspace)


@contextlib.contextmanager
def shared_disk_factory_context(**kwargs):
    tmp_path = tempfile.TemporaryDirectory()
    tmp_storage_path = pathlib.Path(tmp_path.name) / ".shared-storage"
    tmp_storage_path.mkdir(parents=True)

    def shared_disk_factory(name: str) -> SharedDiskRecordTransmitter:
        return SharedDiskRecordTransmitter(
            name=name,
            storage_path=tmp_storage_path,
        )

    try:
        yield shared_disk_factory
    finally:
        tmp_path.cleanup()


@contextlib.contextmanager
def in_memory_factory_context(**kwargs):
    def in_memory_factory(name: str) -> InMemoryRecordTransmitter:
        return InMemoryRecordTransmitter(name=name)

    yield in_memory_factory


@contextlib.contextmanager
def ert_storage_factory_context(storage_path):
    def ert_storage_factory(name: str) -> ert.storage.StorageRecordTransmitter:
        return ert.storage.StorageRecordTransmitter(name=name, storage_url=storage_path)

    yield ert_storage_factory


factory_params = pytest.mark.parametrize(
    ("record_transmitter_factory_context"),
    (
        ert_storage_factory_context,
        in_memory_factory_context,
        shared_disk_factory_context,
    ),
)

simple_records = pytest.mark.parametrize(
    ("data_in", "expected_data"),
    (
        (
            [1, 2, 3],
            [1, 2, 3],
        ),
        (
            (1.0, 10.0, 42.0, 999.0),
            [1.0, 10.0, 42.0, 999.0],
        ),
        (
            [],
            [],
        ),
        (
            [12.0],
            [12.0],
        ),
        (
            {1, 2, 3},
            [1, 2, 3],
        ),
        (
            {"a": 0, "b": 1, "c": 2},
            {"a": 0, "b": 1, "c": 2},
        ),
        (
            {0: 10, 100: 0},
            {0: 10, 100: 0},
        ),
        (
            b"\x00",
            b"\x00",
        ),
        (
            b"\xF0\x9F\xA6\x89",
            b"\xF0\x9F\xA6\x89",
        ),
    ),
)

mime_types = pytest.mark.parametrize(
    ("mime_type"),
    tuple(("application/octet-stream",) + ert.serialization.registered_types()),
)


@pytest.mark.asyncio
@simple_records
@factory_params
async def test_simple_record_transmit(
    record_transmitter_factory_context: ContextManager[
        Callable[[str], RecordTransmitter]
    ],
    data_in,
    expected_data,
    storage_path,
):
    with record_transmitter_factory_context(
        storage_path=storage_path
    ) as record_transmitter_factory:
        transmitter = record_transmitter_factory(name="some_name")
        await transmitter.transmit_data(data_in)
        assert transmitter.is_transmitted()
        with pytest.raises(RuntimeError, match="Record already transmitted"):
            await transmitter.transmit_data([1, 2, 3])


@pytest.mark.asyncio
@simple_records
@factory_params
@mime_types
async def test_simple_record_transmit_from_file(
    record_transmitter_factory_context: ContextManager[
        Callable[[str], RecordTransmitter]
    ],
    data_in,
    expected_data,
    mime_type,
    storage_path,
):
    if isinstance(data_in, bytes) and mime_type != "application/octet-stream":
        pytest.skip(f"unsupported serialization of opaque record to {mime_type}")
    if not isinstance(data_in, bytes) and mime_type == "application/octet-stream":
        pytest.skip(f"unsupported serialization of num record to {mime_type}")
    filename = "record.file"
    with record_transmitter_factory_context(
        storage_path=storage_path
    ) as record_transmitter_factory, tmp():
        transmitter = record_transmitter_factory(name="some_name")
        if mime_type == "application/octet-stream":
            with open(filename, "wb") as fb:
                fb.write(expected_data)
        else:
            with open(filename, "wt", encoding="utf-8") as ft:
                ert.serialization.get_serializer(mime_type).encode_to_file(
                    expected_data, ft
                )
        await transmitter.transmit_file(filename, mime=mime_type)
        assert transmitter.is_transmitted()
        with pytest.raises(RuntimeError, match="Record already transmitted"):
            await transmitter.transmit_file(filename, mime=mime_type)


@pytest.mark.asyncio
@simple_records
@factory_params
async def test_simple_record_transmit_and_load(
    record_transmitter_factory_context: ContextManager[
        Callable[[str], RecordTransmitter]
    ],
    data_in,
    expected_data,
    storage_path,
):
    with record_transmitter_factory_context(
        storage_path=storage_path
    ) as record_transmitter_factory:
        transmitter = record_transmitter_factory(name="some_name")
        await transmitter.transmit_data(data_in)

        record = await transmitter.load()
        assert record.data == expected_data


@pytest.mark.asyncio
@simple_records
@factory_params
@mime_types
async def test_simple_record_transmit_and_dump(
    record_transmitter_factory_context: ContextManager[
        Callable[[str], RecordTransmitter]
    ],
    data_in,
    expected_data,
    mime_type,
    storage_path,
):
    if isinstance(data_in, bytes) and mime_type != "application/octet-stream":
        pytest.skip(f"unsupported serialization of opaque record to {mime_type}")
    if not isinstance(data_in, bytes) and mime_type == "application/octet-stream":
        pytest.skip(f"unsupported serialization of num record to {mime_type}")
    with record_transmitter_factory_context(
        storage_path=storage_path
    ) as record_transmitter_factory, tmp():
        transmitter = record_transmitter_factory(name="some_name")
        await transmitter.transmit_data(data_in)

        await transmitter.dump("record", mime_type)
        if mime_type == "application/octet-stream":
            with open("record", "rb") as f:
                assert expected_data == f.read()
        else:
            with open("record", "rt", encoding="utf-8") as f:
                assert (
                    ert.serialization.get_serializer(mime_type).encode(expected_data)
                    == f.read()
                )


@pytest.mark.asyncio
@simple_records
@factory_params
async def test_simple_record_transmit_pickle_and_load(
    record_transmitter_factory_context: ContextManager[
        Callable[[str], RecordTransmitter]
    ],
    data_in,
    expected_data,
    storage_path,
):
    with record_transmitter_factory_context(
        storage_path=storage_path
    ) as record_transmitter_factory:
        transmitter = record_transmitter_factory(name="some_name")
        transmitter = pickle.loads(cloudpickle.dumps(transmitter))
        await transmitter.transmit_data(data_in)
        transmitter = pickle.loads(cloudpickle.dumps(transmitter))
        record = await transmitter.load()

        assert record.data == expected_data


@pytest.mark.asyncio
@factory_params
async def test_load_untransmitted_record(
    record_transmitter_factory_context: ContextManager[
        Callable[[str], RecordTransmitter]
    ],
):
    with record_transmitter_factory_context(
        storage_path=None
    ) as record_transmitter_factory:
        transmitter = record_transmitter_factory(name="some_name")
        with pytest.raises(RuntimeError, match="cannot load untransmitted record"):
            _ = await transmitter.load()


@pytest.mark.asyncio
@factory_params
async def test_dump_untransmitted_record(
    record_transmitter_factory_context: ContextManager[
        Callable[[str], RecordTransmitter]
    ],
):
    with record_transmitter_factory_context(
        storage_path=None
    ) as record_transmitter_factory:
        transmitter = record_transmitter_factory(name="some_name")
        with pytest.raises(RuntimeError, match="cannot dump untransmitted record"):
            await transmitter.dump("some.file", "text/whatever")