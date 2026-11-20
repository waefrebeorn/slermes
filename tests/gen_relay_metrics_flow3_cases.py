#!/usr/bin/env python3
"""Generate tests/fixtures/relay_metrics_flow3.jsonl — the scripted-seam case
corpus for the relay_metrics_flow3 FLOW port (the FIFTH-wave remainder of
hermes_cli/observability/relay_shared_metrics.py). >=5 cases per ported func,
covering happy paths, branch gates, scripted Exception (swallowed by _guarded /
_safe), scripted BaseException-only (passthrough), the full builtin exception
hierarchy (Unicode/ExceptionGroup/OSError families), state windows and
fail-closed missing scripts. Deterministic (no randomness).

Each case: {"func": <live_name>, "in": {...args..., "io": {seam: script},
"state": {...}}}. A bare JSON array script is a per-call QUEUE; a list RESULT is
wrapped {"__script__": true, "ret": [...]}; a raise is
{"__raise__": kind, "message": msg}.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "relay_metrics_flow3.jsonl")

CASES = []


def add(func, **inp):
    CASES.append({"func": func, "in": inp})


def raise_(kind, msg="boom"):
    return {"__raise__": kind, "message": msg}


def cb(name):
    return {"__callable__": name}


def listret(arr):
    return {"__script__": True, "ret": arr}


# ── _text (8) ────────────────────────────────────────────────────────────────
add("_text", event={"s": "abc"}, key="s")
add("_text", event={"s": 7}, key="s")
add("_text", event={"s": True}, key="s")
add("_text", event={}, key="s")
add("_text", event={"s": None}, key="s")
add("_text", event={"s": 0}, key="s")
add("_text", event={"s": ""}, key="s")
add("_text", event={"s": -3}, key="s")

# ── _retry_ordinal (7) ──────────────────────────────────────────────────────
add("_retry_ordinal", event={})
add("_retry_ordinal", event={"retry_count": 3})
add("_retry_ordinal", event={"retry_count": 0})
add("_retry_ordinal", event={"retry_count": -1})
add("_retry_ordinal", event={"retry_count": True})
add("_retry_ordinal", event={"retry_count": 2.5})
add("_retry_ordinal", event={"retry_count": "4"})

# ── _session_pair (6) ───────────────────────────────────────────────────────
add("_session_pair", event={"session_id": "s1", "turn_id": "u1"}, key="turn_id")
add("_session_pair", event={"turn_id": "u1"}, key="turn_id")
add("_session_pair", event={"session_id": "s1"}, key="turn_id")
add("_session_pair", event={"session_id": "", "turn_id": ""}, key="turn_id")
add("_session_pair", event={"session_id": "s2", "task_id": "t2"}, key="task_id")
add("_session_pair", event={"session_id": 5, "turn_id": 6}, key="turn_id")

# ── _identities_compatible (7) ──────────────────────────────────────────────
add("_identities_compatible", candidate=["a", "r1", "c1"], observed=["a", "r1", "c1"])
add("_identities_compatible", candidate=["a", "r1", "c1"], observed=["a", "r1", ""])
add("_identities_compatible", candidate=["a", "r1", "c1"], observed=["a", "r1", "c2"])
add("_identities_compatible", candidate=["", "", "c1"], observed=["a", "r1", "c1"])
add("_identities_compatible", candidate=["b", "r1", "c1"], observed=["", "", "c1"])
add("_identities_compatible", candidate=["", "", "c1"], observed=["", "", "c1"])
add("_identities_compatible", candidate=["b", "r9", "c1"], observed=["a", "r1", "c1"])

# ── _event_matches_task_turn (6) ────────────────────────────────────────────
add("_event_matches_task_turn", task={"retired_turn_ids": [], "turn_ids": []}, event={})
add("_event_matches_task_turn", task={"retired_turn_ids": ["t1"], "turn_ids": ["t1"]},
    event={"turn_id": "t1"})
add("_event_matches_task_turn", task={"retired_turn_ids": [], "turn_ids": []},
    event={"turn_id": "t9"})
add("_event_matches_task_turn", task={"retired_turn_ids": [], "turn_ids": ["t1", "t2"]},
    event={"turn_id": "t1"})
add("_event_matches_task_turn", task={"retired_turn_ids": [], "turn_ids": ["t1"]},
    event={"turn_id": "t9"})
add("_event_matches_task_turn", task={"retired_turn_ids": ["t3"], "turn_ids": ["t3"]},
    event={"turn_id": "t3"})

# ── _existing_model_call_key (7) ────────────────────────────────────────────
add("_existing_model_call_key", session={"model_calls": [["t1", "r1"]]}, event={})
add("_existing_model_call_key", session={"model_calls": [["t1", "r1"]]},
    event={"task_id": "t1", "api_request_id": "r1"})
add("_existing_model_call_key", session={"model_calls": [["t1", "r1"]]},
    event={"task_id": "t9", "api_request_id": "r1"})
add("_existing_model_call_key", session={"model_calls": [["t2", "r1"]]},
    event={"api_request_id": "r1"})
add("_existing_model_call_key", session={"model_calls": [["t2", "r1"], ["t3", "r1"]]},
    event={"api_request_id": "r1"})
add("_existing_model_call_key", session={"model_calls": []}, event={"api_request_id": "r1"})
add("_existing_model_call_key", session={"model_calls": [["t1", "r1"], ["t2", "r2"]]},
    event={"task_id": "t2", "api_request_id": "r2"})

# ── _guarded (10) ───────────────────────────────────────────────────────────
_G = "Hermes shared metrics operation failed"
add("_guarded", message=_G, callback=cb("cb"), args=[], kwargs={},
    io={"cb": 42})
add("_guarded", message=_G, callback=cb("cb"), args=[], kwargs={},
    io={"cb": None})
add("_guarded", message=_G, callback=cb("cb"), args=[1, "x"], kwargs={"k": True},
    io={"cb": raise_("ValueError"), "logger.warning": None})
add("_guarded", message=_G, callback=cb("cb"), args=[], kwargs={},
    io={"cb": raise_("SystemExit")})
add("_guarded", message=_G, callback=cb("cb"), args=[], kwargs={},
    io={"cb": raise_("KeyboardInterrupt")})
add("_guarded", message=_G, callback=cb("cb"), args=[], kwargs={},
    io={"cb": raise_("FileNotFoundError"), "logger.warning": None})
add("_guarded", message=_G, callback=cb("cb"), args=[], kwargs={},
    io={"cb": raise_("ExceptionGroup"), "logger.warning": None})
add("_guarded", message=_G, callback=cb("cb"), args=[], kwargs={},
    io={"cb": raise_("BaseExceptionGroup")})
add("_guarded", message=_G, callback=cb("cb"), args=[], kwargs={},
    io={"cb": raise_("ValueError"), "logger.warning": raise_("RuntimeError", "log fail")})
add("_guarded", message=_G, callback=cb("cb"), args=[], kwargs={},
    io={"logger.warning": None})

# ── _safe (6) ───────────────────────────────────────────────────────────────
add("_safe", callback=cb("cb"), args=[], kwargs={}, io={"cb": "ok"})
add("_safe", callback=cb("cb"), args=[], kwargs={},
    io={"cb": raise_("TypeError"), "logger.warning": None})
add("_safe", callback=cb("cb"), args=[], kwargs={}, io={"cb": raise_("SystemExit")})
add("_safe", callback=cb("cb"), args=[], kwargs={}, io={"cb": listret(["a", "b"])})
add("_safe", callback=cb("cb"), args=[], kwargs={},
    io={"cb": raise_("UnicodeDecodeError"), "logger.warning": None})
add("_safe", callback=cb("cb"), args=[], kwargs={},
    io={"cb": raise_("OSError"), "logger.warning": None})

# ── _export (8) ─────────────────────────────────────────────────────────────
_STORE = "store.create_and_export_package_if_due"
_SEND = "_send_exported_packages"
add("_export", io={_STORE: None})
add("_export", io={_STORE: {"id": "pkg"}, _SEND: None})
add("_export", io={_STORE: raise_("RuntimeError"), "logger.warning": None})
add("_export", io={_STORE: raise_("SystemExit")})
add("_export", io={_STORE: {"id": "pkg"}, _SEND: raise_("ValueError"),
                   "logger.warning": None})
add("_export", io={_STORE: 0, _SEND: None})
add("_export", io={_STORE: {"id": "pkg"}, _SEND: "sent"})
add("_export", io={"logger.warning": None})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for case in CASES:
            f.write(json.dumps(case, ensure_ascii=False, separators=(",", ":")) + "\n")
    by_func = {}
    for c in CASES:
        by_func[c["func"]] = by_func.get(c["func"], 0) + 1
    print("wrote %d cases to %s" % (len(CASES), OUT))
    print("per-func: " + json.dumps(by_func, sort_keys=True))


if __name__ == "__main__":
    main()
