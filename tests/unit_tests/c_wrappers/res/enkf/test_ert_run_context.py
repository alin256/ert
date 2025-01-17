import pytest

from ert._c_wrappers.enkf import RunContext
from ert._c_wrappers.enkf.runpaths import Runpaths


def test_case_init():
    mask = [True] * 100
    RunContext(None, mask=mask)


@pytest.mark.usefixtures("use_tmpdir")
def test_create():
    mask = [True] * 100
    mask[50] = False
    runpaths = Runpaths(
        runpath_format="path/to/sim%d",
        job_name_format="job%d",
    )
    itr = 0
    realizations = list(range(len(mask)))
    run_context1 = RunContext(
        None,
        None,
        mask,
        runpaths.get_paths(realizations, itr),
        runpaths.get_jobnames(realizations, itr),
        itr,
    )
    run_id1 = run_context1.run_id

    run_arg0 = run_context1[0]
    with pytest.raises(ValueError):
        run_arg0.getQueueIndex()

    assert run_arg0.iter_id == itr
    assert run_arg0.get_run_id() == run_id1

    run_context2 = RunContext(
        None,
        None,
        mask,
        runpaths.get_paths(realizations, itr),
        runpaths.get_jobnames(realizations, itr),
        itr,
    )
    run_id2 = run_context2.run_id

    assert run_id1 != run_id2

    assert run_context1.is_active(49)
    assert not run_context1.is_active(50)
