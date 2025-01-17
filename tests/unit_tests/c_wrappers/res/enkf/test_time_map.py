import datetime

import pandas as pd
import pytest
from ecl.summary import EclSum

from ert._c_wrappers.enkf import TimeMap


def test_time_map():
    # pylint: disable=pointless-statement
    with pytest.raises(IOError):
        TimeMap("Does/not/exist")

    tm = TimeMap()
    with pytest.raises(IndexError):
        tm[10]

    assert repr(tm).startswith("TimeMap(")

    assert tm.update(0, datetime.date(2000, 1, 1))
    assert tm[0] == datetime.date(2000, 1, 1)

    with pytest.raises(Exception):
        tm.update(0, datetime.date(2000, 1, 2))

    assert tm.update(1, datetime.date(2000, 1, 2))
    assert tm.dump() == [
        (0, datetime.date(2000, 1, 1), 0),
        (1, datetime.date(2000, 1, 2), 1),
    ]


def test_fscanf(tmp_path):
    tm = TimeMap()

    with pytest.raises(IOError):
        tm.fload("Does/not/exist")

    with open(tmp_path / "map1.txt", "w") as fileH:
        fileH.write("2000-10-10\n")
        fileH.write("2000-10-12\n")
        fileH.write("2000-10-14\n")
        fileH.write("2000-10-16\n")

    tm.fload(str(tmp_path / "map1.txt"))
    assert len(tm) == 4
    assert tm[0] == datetime.date(2000, 10, 10)
    assert tm[3] == datetime.date(2000, 10, 16)

    with open(str(tmp_path / "map2.txt"), "w") as fileH:
        # First date will trigger a DeprecationWarning
        # on legacy date format
        fileH.write("10/10/2000\n")
        fileH.write("2000-10-12\n")
        fileH.write("2000-10-14\n")
        fileH.write("2000-10-16\n")

    tm.fload(str(tmp_path / "map2.txt"))
    assert len(tm) == 4
    assert tm[0] == datetime.date(2000, 10, 10)
    assert tm[3] == datetime.date(2000, 10, 16)

    with open(tmp_path / "map3.txt", "w") as fileH:
        fileH.write("10/10/200X\n")

    with pytest.raises(Exception):
        tm.fload(str(tmp_path / "map3.txt"))

    assert len(tm) == 4
    assert tm[0] == datetime.date(2000, 10, 10)
    assert tm[3] == datetime.date(2000, 10, 16)

    with open(tmp_path / "map4.txt", "w") as fileH:
        fileH.write("2000-10-12\n")
        fileH.write("2000-10-10\n")

    with pytest.raises(Exception):
        tm.fload(str(tmp_path / "map4.txt"))

    assert len(tm) == 4
    assert tm[0] == datetime.date(2000, 10, 10)
    assert tm[3] == datetime.date(2000, 10, 16)


def test_setitem():
    tm = TimeMap()
    tm[0] = datetime.date(2000, 1, 1)
    tm[1] = datetime.date(2000, 1, 2)
    assert len(tm) == 2

    assert tm[0] == datetime.date(2000, 1, 1)
    assert tm[1] == datetime.date(2000, 1, 2)


def test_in():
    tm = TimeMap()
    tm[0] = datetime.date(2000, 1, 1)
    tm[1] = datetime.date(2000, 1, 2)
    tm[2] = datetime.date(2000, 1, 3)

    assert datetime.date(2000, 1, 1) in tm
    assert datetime.date(2000, 1, 2) in tm
    assert datetime.date(2000, 1, 3) in tm

    assert datetime.date(2001, 1, 3) not in tm
    assert datetime.date(1999, 1, 3) not in tm


def test_lookupDate():
    tm = TimeMap()
    tm[0] = datetime.date(2000, 1, 1)
    tm[1] = datetime.date(2000, 1, 2)
    tm[2] = datetime.date(2000, 1, 3)

    assert tm.lookupTime(datetime.date(2000, 1, 1)) == 0
    assert tm.lookupTime(datetime.datetime(2000, 1, 1, 0, 0, 0)) == 0

    assert tm.lookupTime(datetime.date(2000, 1, 3)) == 2
    assert tm.lookupTime(datetime.datetime(2000, 1, 3, 0, 0, 0)) == 2

    with pytest.raises(ValueError):
        tm.lookupTime(datetime.date(1999, 10, 10))


def test_lookupDays():
    tm = TimeMap()

    with pytest.raises(ValueError):
        tm.lookupDays(0)

    tm[0] = datetime.date(2000, 1, 1)
    tm[1] = datetime.date(2000, 1, 2)
    tm[2] = datetime.date(2000, 1, 3)

    assert tm.lookupDays(0) == 0
    assert tm.lookupDays(1) == 1
    assert tm.lookupDays(2) == 2

    with pytest.raises(ValueError):
        tm.lookupDays(-1)

    with pytest.raises(ValueError):
        tm.lookupDays(0.50)

    with pytest.raises(ValueError):
        tm.lookupDays(3)


def test_nearest_date_lookup():
    tm = TimeMap()
    with pytest.raises(ValueError):
        tm.lookupTime(datetime.date(1999, 1, 1))

    with pytest.raises(ValueError):
        tm.lookupTime(
            datetime.date(1999, 1, 1),
            tolerance_seconds_before=10,
            tolerance_seconds_after=10,
        )

    tm[0] = datetime.date(2000, 1, 1)
    tm[1] = datetime.date(2000, 2, 1)
    tm[2] = datetime.date(2000, 3, 1)

    # Outside of total range will raise an exception, irrespective of
    # the tolerances used.
    with pytest.raises(ValueError):
        tm.lookupTime(
            datetime.date(1999, 1, 1),
            tolerance_seconds_before=-1,
            tolerance_seconds_after=-1,
        )

    with pytest.raises(ValueError):
        tm.lookupTime(
            datetime.date(2001, 1, 1),
            tolerance_seconds_before=-1,
            tolerance_seconds_after=-1,
        )

    assert (
        tm.lookupTime(
            datetime.datetime(2000, 1, 1, 0, 0, 10), tolerance_seconds_after=15
        )
        == 0
    )
    assert (
        tm.lookupTime(
            datetime.datetime(2000, 1, 1, 0, 0, 10),
            tolerance_seconds_before=3600 * 24 * 40,
        )
        == 1
    )

    assert (
        tm.lookupTime(
            datetime.date(2000, 1, 10),
            tolerance_seconds_before=-1,
            tolerance_seconds_after=-1,
        )
        == 0
    )
    assert (
        tm.lookupTime(
            datetime.date(2000, 1, 20),
            tolerance_seconds_before=-1,
            tolerance_seconds_after=-1,
        )
        == 1
    )

    with pytest.raises(ValueError):
        tm.lookupTime(
            datetime.date(2001, 10, 1),
            tolerance_seconds_before=10,
            tolerance_seconds_after=10,
        )


@pytest.mark.parametrize(
    "time_steps, expected",
    [
        pytest.param(
            {"2000-01-01": 1.0, "2000-01-03": 4.0, "2000-01-04": 1.0},
            "",
            id="Everything matches",
        ),
        pytest.param(
            {
                "2000-01-01": 0.0,
                "2000-01-02": 0.5,
                "2000-01-03": 0.5,
                "2000-01-04": 1.0,
            },
            (
                "2 inconsistencies in time_map, first: Time mismatch for step: 2, "
                "response time: 2000-01-02, reference case: 2000-01-03, last: Time "
                "mismatch for step: 3, response time: 2000-01-03, reference case: "
                "2000-01-04"
            ),
            id="Extra time step in the middle",
        ),
        pytest.param(
            {"2000-01-01": 1.0, "2000-01-04": 1.0},
            (
                "1 inconsistencies in time_map, first: Time mismatch for step: 2, "
                "response time: 2000-01-04, reference case: 2000-01-03, last: Time "
                "mismatch for step: 2, response time: 2000-01-04, reference case: "
                "2000-01-03"
            ),
            id="Missing time step in the middle",
        ),
    ],
)
def test_time_map_summary_update(time_steps, expected):

    df = pd.DataFrame(
        {"FOPR": {"2000-01-01": 0.0, "2000-01-03": 1.0, "2000-01-04": 1.0}}
    )
    df.index = pd.to_datetime(df.index)
    refcase = EclSum.from_pandas("REFCASE", df)

    time_map = TimeMap()
    time_map.attach_refcase(refcase)

    df = pd.DataFrame({"FOPR": time_steps})
    df.index = pd.to_datetime(df.index)
    summary = EclSum.from_pandas("ECL_CASE", df)

    assert time_map.summary_update(summary) == expected
