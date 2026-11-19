#!/usr/bin/env python3
"""Generate tests/fixtures/update_lock_flow.jsonl.

Inputs only (no expected values): the oracle recomputes the live reference.
Scripted seam results live in in.io (see the harness header). Paths are
POSIX strings; u64 values past 2^53 use {"__bigint__": decimal} / limit_str.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "update_lock_flow.jsonl")

CASES = []
W = {"pid": 100, "ct": 1000.0, "now": 2000.0}
PATH = "/h/.hermes-update-in-progress"


def add(func, **inp):
    CASES.append({"func": func, "in": inp})


def script(ret=None, raise_spec=None, holder=None):
    s = {"__script__": True, "ret": ret}
    if raise_spec is not None:
        s["raise"] = raise_spec
    if holder is not None:
        s["holder"] = holder
    return s


def marker(raw="200\n1999\nct:1999\n", pid=200, started_at=1999, create_time=1999.0,
           delegate_pid=None, delegate_create_time=None, in_flight=False,
           ct_text="1999", delegate_ct_text=None, runs=()):
    return {
        "raw": raw, "pid": pid, "started_at": started_at, "create_time": create_time,
        "delegate_pid": delegate_pid, "delegate_create_time": delegate_create_time,
        "in_flight": in_flight, "ct_text": ct_text, "delegate_ct_text": delegate_ct_text,
        "runs": list(runs),
    }


def world_io(alive=True, ct_of=1999.0, now=2000.0):
    return {
        "real_world": {"pid": 100, "ct": 1000.0, "now": now},
        "world.alive": alive, "world.ct_of": ct_of, "time.time": now,
    }


# ── _bounded_int ──────────────────────────────────────────────────────────
add("_bounded_int", text="123", limit=4294967295)
add("_bounded_int", text="0000", limit=4294967295)
add("_bounded_int", text="", limit=4294967295)
add("_bounded_int", text="12a", limit=4294967295)
add("_bounded_int", text="4294967295", limit=4294967295)
add("_bounded_int", text="4294967296", limit=4294967295)
add("_bounded_int", text="99999999999", limit=4294967295)
add("_bounded_int", text="-1", limit=4294967295)
add("_bounded_int", text=" 1", limit=4294967295)
add("_bounded_int", text="18446744073709551615", limit_str="18446744073709551615")
add("_bounded_int", text="18446744073709551616", limit_str="18446744073709551615")
add("_bounded_int", text="99999999999999999999", limit_str="18446744073709551615")
add("_bounded_int", text="1844674407370955161", limit_str="18446744073709551615")
add("_bounded_int", text="00018446744073709551615", limit_str="18446744073709551615")
add("_bounded_int", text="1700000000", limit_str="18446744073709551615")

# ── _as_ct ────────────────────────────────────────────────────────────────
add("_as_ct", recorded=None)
add("_as_ct", recorded=123)
add("_as_ct", recorded=1.5)
add("_as_ct", recorded=True)
add("_as_ct", recorded="ct:123.456")
add("_as_ct", recorded="  789  ")
add("_as_ct", recorded="ct:")
add("_as_ct", recorded="ct:ct:1")
add("_as_ct", recorded="abc")
add("_as_ct", recorded="12.34.56")
add("_as_ct", recorded="")
add("_as_ct", recorded="ct:00123")

# ── _incarnation ──────────────────────────────────────────────────────────
add("_incarnation", pid=0, recorded=None, world=W, io={})
add("_incarnation", pid=-5, recorded=1.0, world=W, io={})
add("_incarnation", pid=100, recorded=None, world=W, io={})
add("_incarnation", pid=100, recorded=1000.004, world=W, io={})
add("_incarnation", pid=100, recorded=1000.006, world=W, io={})
add("_incarnation", pid=100, recorded=999.0, world=W, io={})
add("_incarnation", pid=100, recorded=1000.0, world={"pid": 100, "ct": 1000.7, "now": 2000.0}, io={})
add("_incarnation", pid=100, recorded=None, world={"pid": 100, "ct": None, "now": 2000.0}, io={})
add("_incarnation", pid=100, recorded=5.0, world={"pid": 100, "ct": None, "now": 2000.0}, io={})
add("_incarnation", pid=200, recorded=1000.0, world=W, io={"world.alive": False})
add("_incarnation", pid=200, recorded=None, world=W, io={"world.alive": True})
add("_incarnation", pid=200, recorded=1000.0, world=W, io={"world.alive": True, "world.ct_of": None})
add("_incarnation", pid=200, recorded=1000.0, world=W, io={"world.alive": True, "world.ct_of": 1001.5})
add("_incarnation", pid=200, recorded=1000.0, world=W, io={"world.alive": True, "world.ct_of": 1002.5})
add("_incarnation", pid=200, recorded=1000.0, world=W, io={"world.alive": True, "world.ct_of": 998.0})
add("_incarnation", pid=200, recorded=1000.0, world=W,
    io={"world.alive": script(raise_spec={"type": "RuntimeError", "message": "probe boom"})})

# ── _identity_live ────────────────────────────────────────────────────────
add("_identity_live", pid=200, create_time=None, age=1199.0, world=W, io={"world.alive": True})
add("_identity_live", pid=200, create_time=None, age=1201.0, world=W, io={"world.alive": True})
add("_identity_live", pid=200, create_time=None, age=1200.0, world=W, io={"world.alive": True})
add("_identity_live", pid=100, create_time=1000.001, age=99999.0, world=W, io={})
add("_identity_live", pid=0, create_time=None, age=0.0, world=W, io={})
add("_identity_live", pid=200, create_time=1999.0, age=5.0, world=W,
    io={"world.alive": True, "world.ct_of": None})

# ── _parse_marker ─────────────────────────────────────────────────────────
add("_parse_marker", raw="123\n1700000000\nct:1700000000.123\n")
add("_parse_marker", raw="123\n1700000000\nbad\n")
add("_parse_marker", raw="123\n1700000000\nct:1.5\ndelegate:456 ct:2.5\nrun:abc\nrun:x.y-z_1\n")
add("_parse_marker", raw=" 123 \n\t1700000000\t\n ct:1.5 \n")
add("_parse_marker", raw="123\r\n1700000000\r\nct:1.5\r\n")
add("_parse_marker", raw="123\n1700000000\nct:1.5\n")
add("_parse_marker", raw="abc\n1700000000\n")
add("_parse_marker", raw="123\nnotanint\n")
add("_parse_marker", raw="123\n1700000000\nct:1.5\ndelegate:99999999999 ct:1\ndelegate:456 ct:2\n")
add("_parse_marker", raw="", mtime=1995.0, io={"time.time": 2000.0})
add("_parse_marker", raw="", mtime=1996.0, io={"time.time": 2000.0})
add("_parse_marker", raw="", mtime=None)
add("_parse_marker", raw="123")
add("_parse_marker", raw="1\n2\n3\n4\n")
add("_parse_marker", raw="123\n18446744073709551615\nct:1\n")
add("_parse_marker", raw="123\n1700000000\nct:1.5\nrun:\nrun:" + "a" * 129 + "\ndelegate:456 ct:bad\n")
add("_parse_marker", raw_hex="313233ff")
add("_parse_marker", raw_hex="efbbbf3132330a313730303030303030300a63743a310a")

# ── judge_marker ──────────────────────────────────────────────────────────
add("judge_marker", raw="x\ny\n", world=W, io={})
add("judge_marker", raw="200\n1999\nct:1999\n", world=W, io={"world.alive": False})
add("judge_marker", raw="200\n1999\nct:1999\nrun:r1\n", world=W,
    io={"world.alive": True, "world.ct_of": 1999.0})
add("judge_marker", raw="100\n1999\nct:1000.001\n", world=W, io={})
add("judge_marker", raw="200\n1999\nct:1999\ndelegate:100 ct:1000.001\n", world=W,
    io={"world.alive": True, "world.ct_of": 1999.0})
add("judge_marker", raw="200\n1999\n", world=W, io={"world.alive": True})
add("judge_marker", raw="200\n100\n", world=W, io={"world.alive": True})
add("judge_marker", raw="200\n1999\nct:1999\ndelegate:300 ct:1999\n", world=W,
    io={"world.alive": [False, True], "world.ct_of": 1999.0})
add("judge_marker", raw="100\n1999\n", world=W, io={})  # v1 naming us: not ours

# ── _release_decision ─────────────────────────────────────────────────────
add("_release_decision", raw="x\ny\n", world=W, io={})
add("_release_decision", raw="100\n1999\nct:1000.001\n", world=W, io={})
add("_release_decision", raw="100\n1999\nct:1000.001\ndelegate:200 ct:1999\n", world=W,
    io={"world.alive": True, "world.ct_of": 1999.0})
add("_release_decision", raw="100\n1999\nct:1000.001\ndelegate:200 ct:1999\n", world=W,
    io={"world.alive": False})
add("_release_decision", raw="100\n1999\nct:1000.001\ndelegate:100 ct:1000.001\n", world=W, io={})
add("_release_decision", raw="200\n1999\nct:1999\ndelegate:100 ct:1000.001\nrun:r1\n", world=W,
    io={"world.alive": True, "world.ct_of": 1999.0})
add("_release_decision", raw="200\n1999\nct:1999\ndelegate:100 ct:1000.001\n", world=W,
    io={"world.alive": False})
add("_release_decision", raw="200\n1999\nct:1999\n", world=W, io={"world.alive": True, "world.ct_of": 1999.0})
add("_release_decision", raw="100\n1999\nct:1000.001\nrun:r1\nrun:r2\n", world=W, io={})
add("_release_decision", raw="100\n1999\n", world=W, io={})

# ── _live_partners ────────────────────────────────────────────────────────
add("_live_partners", marker=marker(in_flight=True), io={})
add("_live_partners", marker=marker(), io=world_io())
add("_live_partners",
    marker=marker(delegate_pid=300, delegate_create_time=1999.0, delegate_ct_text="1999",
                  raw="200\n1999\nct:1999\ndelegate:300 ct:1999\n"),
    io=world_io(alive=[False, True]))
add("_live_partners",
    marker=marker(delegate_pid=300, delegate_create_time=1999.0, delegate_ct_text="1999",
                  raw="200\n1999\nct:1999\ndelegate:300 ct:1999\n"),
    io=world_io(alive=True))
add("_live_partners",
    marker=marker(delegate_pid=300, delegate_create_time=1999.0, delegate_ct_text="1999",
                  raw="200\n1999\nct:1999\ndelegate:300 ct:1999\n"),
    io=world_io(alive=False))
add("_live_partners", marker=marker(create_time=None, ct_text=None, raw="200\n1999\n"),
    io=world_io())
add("_live_partners", marker=marker(started_at=None, create_time=None, ct_text=None, raw="200\n"), io={})

# ── _reclaim_dead ─────────────────────────────────────────────────────────
def reclaim_io(**over):
    io = {
        "mutex.enter": script(None), "mutex.exit": script(None),
        "_read_marker": None, "checkout_lock_held": False, "path.unlink": script(None),
    }
    io.update(world_io())
    io.update(over)
    return io


add("_reclaim_dead", path=PATH, install_root="/install", io=reclaim_io())
add("_reclaim_dead", path=PATH, install_root="/install",
    io=reclaim_io(_read_marker=marker()))
add("_reclaim_dead", path=PATH, install_root="/install",
    io=reclaim_io(_read_marker=marker(), **{"world.alive": False, "checkout_lock_held": True}))
add("_reclaim_dead", path=PATH, install_root="/install",
    io=reclaim_io(_read_marker=marker(), **{"world.alive": False}))
add("_reclaim_dead", path=PATH, install_root="/install",
    io=reclaim_io(_read_marker=marker(), **{
        "world.alive": False,
        "path.unlink": script(raise_spec={"type": "FileNotFoundError", "message": "gone"}),
    }))
add("_reclaim_dead", path=PATH, install_root="/install",
    io=reclaim_io(**{"mutex.enter": script(raise_spec={"type": "MarkerBusy", "message": PATH + " is held by another update"})}))
add("_reclaim_dead", path=PATH, install_root="/install",
    io=reclaim_io(**{"mutex.enter": script(raise_spec={"type": "PermissionError", "message": "denied", "errno": 13})}))
add("_reclaim_dead", path=PATH, install_root="/install",
    io=reclaim_io(_read_marker=script(raise_spec={"type": "OSError", "message": "read boom", "errno": 5})))
add("_reclaim_dead", path=PATH, install_root="/install",
    io=reclaim_io(_read_marker=marker(), **{
        "world.alive": False,
        "path.unlink": script(raise_spec={"type": "PermissionError", "message": "denied", "errno": 13}),
    }))
add("_reclaim_dead", path=PATH, install_root="/install",
    io=reclaim_io(_read_marker=marker(), **{
        "world.alive": False,
        "checkout_lock_held": script(raise_spec={"type": "RuntimeError", "message": "probe boom"}),
    }))
add("_reclaim_dead", path=PATH, install_root=None,
    io=reclaim_io(_read_marker=marker(started_at=None, create_time=None, ct_text=None, pid=-1, raw="x\n")))

# ── _claim_marker ─────────────────────────────────────────────────────────
# NOTE: seams whose RETURN VALUE is a list (legacy_profile_claims,
# _live_partners) must be wrapped in script(ret=[...]): a bare JSON array in
# in.io is a call-QUEUE (one element consumed per call), so a bare [] reads
# as "script exhausted" and a bare [spec] returns its first element instead
# of the list. script() returns the list verbatim on every call.
def claim_io(**over):
    io = {
        "path.parent.mkdir": script(None),
        "_sweep_dead_tmp_siblings": script(None),
        "legacy_profile_claims": script([]),
        "os.getpid": 100,
        "time.time": 1000.0,
        "_identity_line": "ct:1000.000",
        "_publish_exclusive": True,
        "mutex.enter": script(None), "mutex.exit": script(None),
        "_read_marker": None,
        "_live_partners": script([]),
        "self._checkout_held_elsewhere": False,
        "path.unlink": script(None),
        "read_live_update": None,
    }
    io.update(over)
    return io


def claim_self(**over):
    s = {"path": PATH, "install_root": None, "_checkout": False, "acquired": False, "holder": None}
    s.update(over)
    return s


add("_claim_marker", self=claim_self(), claims={}, io=claim_io())
add("_claim_marker", self=claim_self(), claims={PATH: 1}, io=claim_io())
add("_claim_marker", self=claim_self(), claims={},
    io=claim_io(**{"path.parent.mkdir": script(raise_spec={"type": "PermissionError", "message": "denied", "errno": 13})}))
add("_claim_marker", self=claim_self(), claims={},
    io=claim_io(legacy_profile_claims=script([{"pid": 200, "age_seconds": 5.0}]),
                **{"self._is_partner": False}))
add("_claim_marker", self=claim_self(), claims={},
    io=claim_io(legacy_profile_claims=script([{"pid": 100, "age_seconds": 5.0}]),
                **{"self._is_partner": True}))
add("_claim_marker", self=claim_self(), claims={},
    io=claim_io(_publish_exclusive=[False, False, False], _read_marker=[None, None, None]))
add("_claim_marker", self=claim_self(), claims={},
    io=claim_io(_publish_exclusive=False, _read_marker=marker(), _live_partners=script([200]),
                **{"self._adopt_or_refuse": script(False, holder={"pid": 200, "age_seconds": 5.0})}))
add("_claim_marker", self=claim_self(), claims={},
    io=claim_io(_publish_exclusive=False, _read_marker=marker(), _live_partners=script([200]),
                **{"self._adopt_or_refuse": script(True)}))
add("_claim_marker", self=claim_self(), claims={},
    io=claim_io(_publish_exclusive=False, _read_marker=marker(),
                _live_partners=script([]), **{"self._checkout_held_elsewhere": True,
                                      "time.time": [1000.0, 2000.0]}))
add("_claim_marker", self=claim_self(), claims={},
    io=claim_io(_publish_exclusive=[False, True], _read_marker=marker(), _live_partners=script([])))
add("_claim_marker", self=claim_self(), claims={},
    io=claim_io(_publish_exclusive=False, _read_marker=marker(), _live_partners=script([]),
                **{"path.unlink": script(raise_spec={"type": "PermissionError", "message": "denied", "errno": 13})}))
add("_claim_marker", self=claim_self(), claims={},
    io=claim_io(_publish_exclusive=False,
                **{"mutex.enter": script(raise_spec={"type": "MarkerBusy", "message": PATH + " is held by another update"})}))
add("_claim_marker", self=claim_self(), claims={},
    io=claim_io(_publish_exclusive=script(raise_spec={"type": "OSError", "message": "no space", "errno": 28})))
add("_claim_marker", self=claim_self(), claims={},
    io=claim_io(**{"_sweep_dead_tmp_siblings": script(raise_spec={"type": "RuntimeError", "message": "sweep boom"})}))
add("_claim_marker", self=claim_self(), claims={},
    io=claim_io(_publish_exclusive=[False, False, False], _read_marker=[None, None, None],
                read_live_update={"pid": 200, "age_seconds": 3.0}))
add("_claim_marker", self=claim_self(), claims={},
    io=claim_io(_publish_exclusive=[False, False, False, False, False, False],
                _read_marker=[marker(started_at=None, create_time=None, ct_text=None, pid=-1, raw="x\n")] * 3,
                _live_partners=script([])))
add("_claim_marker", self=claim_self(), claims={},
    io=claim_io(_publish_exclusive=[False, True], _read_marker=marker(), _live_partners=script([]),
                **{"path.unlink": script(raise_spec={"type": "FileNotFoundError", "message": "gone"})}))
add("_claim_marker", self=claim_self(install_root="/install"), claims={},
    io=claim_io(_publish_exclusive=[False, False, False], _read_marker=[None, None, None],
                read_live_update=None))
add("_claim_marker", self=claim_self(), claims={},
    io=claim_io(legacy_profile_claims=script([{"pid": 200, "age_seconds": 5.0}, {"pid": 300, "age_seconds": 7.0}]),
                **{"self._is_partner": [False, True]}))
add("_claim_marker", self=claim_self(), claims={},
    io=claim_io(_publish_exclusive=False, _read_marker=marker(), _live_partners=script([0]),
                **{"self._adopt_or_refuse": script(False, holder={"pid": 0, "age_seconds": 0.0, "held": True})}))


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="") as fh:
        for c in CASES:
            fh.write(json.dumps(c, ensure_ascii=False) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print(f"wrote {len(CASES)} cases -> {OUT}")
    for f, n in sorted(counts.items()):
        print(f"  {f}: {n}")
    assert all(n >= 4 for n in counts.values()), "every port needs >=4 cases"


if __name__ == "__main__":
    main()
