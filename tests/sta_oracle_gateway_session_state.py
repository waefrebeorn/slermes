"""Differential oracle for gateway/session_state.py.

Reads the C driver's JSON lines from stdin and compares each against the
expected value computed from LIVE Python (the parent repo's gateway/session_state.py).
Prints `gateway/session_state oracle: N cases, M mismatches` and one
`MISMATCH case=... PY=... C=...` line per divergence. Exit code 0 on full match.
"""

import sys
import json
import os

# Load the live Python module from the parent repo (the oracle/spec).
_PARENT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
if _PARENT not in sys.path:
    sys.path.insert(0, _PARENT)

from gateway import session_state as ss  # noqa: E402


def build_runner():
    """Bare object with the attributes the views read off `self`."""
    class R:
        def __init__(self):
            self._sessions = {}
        def _session_state(self, key):
            s = self._sessions.get(key)
            if s is None:
                s = ss.SessionState()
                self._sessions[key] = s
            return s
    return R()


def jb(v):
    return "true" if v else "false"


def main():
    raw = sys.stdin.read()
    cases = [json.loads(line) for line in raw.splitlines() if line.strip()]

    mismatches = 0
    py_cache = {}  # case -> expected values computed once

    # Build expected values per case by replaying equivalent Python ops.
    # Each helper reconstructs a fresh runner and the relevant view.
    def run_case(case):
        r = build_runner()
        if case == "turn_clear":
            s = r._session_state("k")
            s.turn.agent = "AGENT"
            s.turn.started_ts = 123.0
            s.turn.lease = "LEASE"
            s.turn.lease_token = "TOK"
            s.turn.lease_generation = 7
            s.turn.clear()
            return {
                "agent_null": s.turn.agent is None,
                "started_zero": s.turn.started_ts == 0.0,
                "lease_null": s.turn.lease is None,
                "lease_token_kept": s.turn.lease_token == "TOK",
                "lease_gen_kept": s.turn.lease_generation == 7,
            }
        if case == "conv_clear":
            s = r._session_state("k")
            s.conversation.model_override = "M"
            s.conversation.service_tier_override = "priority"
            s.conversation.last_resolved_model = "gpt"
            import collections.abc
            s.conversation.queued_events = ["e"]
            s.conversation.clear()
            return {
                "model_null": s.conversation.model_override is None,
                "tier_unset": s.conversation.service_tier_override is ss.SERVICE_TIER_UNSET,
                "last_empty": s.conversation.last_resolved_model == "",
                "queued_empty": len(s.conversation.queued_events) == 0,
            }
        if case == "view_getset":
            v = ss.SessionFieldView(r, ss.LEGACY_FIELD_SPECS["_running_agents"])
            v["k1"] = "A"
            got = v.get("k1", None)
            contains = "k1" in v
            miss_rc = None
            try:
                v["nope"]
            except KeyError:
                miss_rc = "KeyError"
            contains_miss = "nope" in v
            return {
                "get_val": got if got is not None else None,
                "rc_ok": got == "A",
                "contains": contains,
                "miss_null": miss_rc == "KeyError",
                "contains_miss": not contains_miss,
            }
        if case == "view_del":
            v = ss.SessionFieldView(r, ss.LEGACY_FIELD_SPECS["_running_agents"])
            v["k1"] = "A"
            d1_ok = True
            try:
                del v["k1"]
            except Exception:
                d1_ok = False
            after = "k1" in v
            d2_rc = "ok"
            try:
                del v["k1"]
            except KeyError:
                d2_rc = "KeyError"
            return {"del_ok": d1_ok, "after_del_absent": not after, "del_absent_rc": 1 if d2_rc == "KeyError" else 0}
        if case == "view_iter":
            v = ss.SessionFieldView(r, ss.LEGACY_FIELD_SPECS["_running_agents"])
            v["k1"] = "A"; v["k2"] = "B"; v["k3"] = "C"
            seen = sorted(v.keys())
            out = {"count": len(seen)}
            for i, k in enumerate(seen):
                out["key%d" % i] = k
            return out
        if case == "view_clear":
            v = ss.SessionFieldView(r, ss.LEGACY_FIELD_SPECS["_running_agents"])
            v["k1"] = "A"; v["k2"] = "B"
            v.clear()
            return {"c1": "k1" not in v, "c2": "k2" not in v}
        if case == "service_tier":
            v = ss.SessionFieldView(r, ss.LEGACY_FIELD_SPECS["_session_service_tier_overrides"])
            def_present = "k1" in v
            v["k1"] = "priority"
            set_present = "k1" in v
            v["k1"] = ss.SERVICE_TIER_UNSET
            unset_present = "k1" in v
            return {"def_present": def_present, "set_present": set_present, "unset_present": unset_present}
        if case == "zero_presence":
            vm = ss.SessionFieldView(r, ss.LEGACY_FIELD_SPECS["_last_resolved_model"])
            vg = ss.SessionFieldView(r, ss.LEGACY_FIELD_SPECS["_session_run_generation"])
            m_def = "k1" in vm
            vm["k1"] = "gpt-4"
            m_set = "k1" in vm
            g_def = "k1" in vg
            vg["k1"] = 5
            g_set = "k1" in vg
            return {"model_def": m_def, "model_set": m_set, "gen_def": g_def, "gen_set": g_set}
        if case == "lease_view":
            v = ss.TurnLeaseTokenView(r)
            kpair = ("k1", 3)
            v[kpair] = "TOK"
            got = v.get(kpair, None)
            kpair2 = ("k1", 9)
            miss = None
            try:
                v[kpair2]
            except KeyError:
                miss = "KeyError"
            contains_ok = kpair in v
            contains_bad = kpair2 in v
            kn = len(list(v.keys()))
            return {
                "set_ok": got == "TOK",
                "get_val": got if got is not None else None,
                "miss_null": miss == "KeyError",
                "contains_ok": contains_ok,
                "contains_bad": not contains_bad,
                "iter_count": kn,
            }
        if case == "lease_del":
            v = ss.TurnLeaseTokenView(r)
            kpair = ("k1", 3)
            v[kpair] = "TOK"
            d1_ok = True
            try:
                del v[kpair]
            except Exception:
                d1_ok = False
            after = kpair in v
            d2_rc = "ok"
            try:
                del v[kpair]
            except KeyError:
                d2_rc = "KeyError"
            return {"del_ok": d1_ok, "after_absent": not after, "del_absent_rc": 1 if d2_rc == "KeyError" else 0}
        return {}

    for c in cases:
        case = c["case"]
        expected = run_case(case)
        # Compare every key in expected against the C output 'c'.
        for k, ev in expected.items():
            if k not in c:
                mismatches += 1
                print("MISMATCH case=%s key=%s PY=%r C=MISSING" % (case, k, ev))
                continue
            cv = c[k]
            # Normalize representation: booleans, strings, numbers.
            if isinstance(ev, bool):
                cbool = (cv == "true" or cv is True)
                eq = (cbool == ev)
            elif isinstance(ev, (int, float)) and not isinstance(ev, bool):
                try:
                    eq = (float(cv) == float(ev))
                except (TypeError, ValueError):
                    eq = False
            else:  # string / None
                eq = (cv == ev)
            if not eq:
                mismatches += 1
                print("MISMATCH case=%s %s PY=%r C=%r" % (case, k, ev, cv))

    print("gateway/session_state oracle: %d cases, %d mismatches" % (len(cases), mismatches))
    sys.exit(1 if mismatches else 0)


if __name__ == "__main__":
    main()
