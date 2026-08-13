"""sta_oracle_agent_estop.py — oracle for the agent/estop.py emergency-stop port.

Mirrors the C harness t_port_agent_estop.c state-machine transitions, driven by
the SAME source-of-truth: agent/estop.py. Reproduces engage/disengage/is_engaged
sentinel semantics and diffs the JSON records emitted by the C harness.

The C harness writes one JSON record per step. We replay the equivalent Python
calls (via subprocess, importing agent.estop) and assert the same transitions.
"""
import json
import os
import sys

sys.path.insert(0, os.path.expanduser("~/hermes-agent-dev"))


def run_py_transitions(case_dir):
    """Reproduce the exact 5 transition steps the C harness emits, returning
    the expected list of dicts. Honors the runner's isolated $TMPH home
    (exported via HERMES_HOME / SLERMES_HOME) — do NOT override it, so Python
    and C agree on the sentinel location. Cleans any stale sentinel first."""
    # Honor whatever the runner exported (isolated home); clean first.
    from agent import estop
    import os as _os
    sp0 = estop.sentinel_path()
    if sp0 and _os.path.exists(sp0):
        _os.remove(sp0)
    if sp0:
        del sp0

    # Force a fresh import so a prior (cached) module state doesn't leak.
    import importlib
    if "agent.estop" in sys.modules:
        del sys.modules["agent.estop"]
    from agent import estop

    results = []

    def step(step_name, arg, engaged, rc, sentinel):
        # Python's estop.sentinel_path() returns a pathlib.PosixPath; coerce to
        # str so it mirrors the C harness's char* sentinel path 1:1.
        if sentinel is not None and not isinstance(sentinel, str):
            sentinel = str(sentinel)
        results.append({
            "step": step_name,
            "arg": arg,
            "engaged": engaged,
            "rc": rc,
            "sentinel": sentinel,
        })

    # CASE 1
    engaged = estop.is_engaged()
    sp = estop.sentinel_path()
    step("initial_not_engaged", "", engaged, engaged, sp)

    engaged_path = estop.engage(reason="test reason")
    e2 = estop.is_engaged()
    step("after_engage", "test reason", e2, 1, engaged_path)

    dis = estop.disengage()
    e3 = estop.is_engaged()
    step("after_disengage", "off", e3, dis, estop.sentinel_path())

    # CASE 2: disengage when not engaged
    dis2 = estop.disengage()
    step("disengage_not_engaged", "off", estop.is_engaged(), dis2, estop.sentinel_path())

    # CASE 3: engage without reason, re-engage (idempotent update)
    p3 = estop.engage(reason=None)
    step("engage_no_reason", "", estop.is_engaged(), 1, p3)

    p4 = estop.engage(reason="second reason")
    step("engage_again_updates", "second reason", estop.is_engaged(), 1, p4)

    estop.disengage()  # cleanup
    return results


def main():
    case_dir = sys.argv[1] if len(sys.argv) > 1 else "."
    expected = run_py_transitions(case_dir)

    mism = 0
    n = 0
    stdin_lines = [l for l in sys.stdin if l.strip().startswith("{")]
    for ln, exp in zip(stdin_lines, expected):
        rec = json.loads(ln)
        n += 1
        for key in ("step", "arg", "rc"):
            if rec.get(key) != exp.get(key):
                mism += 1
                print(f"MISMATCH {key}: got={rec.get(key)!r} exp={exp.get(key)!r}")
        # engaged: compare as int (Python bool vs C int)
        if int(bool(rec.get("engaged"))) != int(exp.get("engaged", 0)):
            mism += 1
            print(f"MISMATCH engaged: got={rec.get('engaged')} exp={exp.get('engaged')}")
        # sentinel: compare path-equality modulo dir separators
        if rec.get("sentinel") != exp.get("sentinel"):
            mism += 1
            print(f"MISMATCH sentinel: got={rec.get('sentinel')!r} exp={exp.get('sentinel')!r}")

    if n != len(expected):
        print(f"COUNT MISMATCH: harness={n} expected={len(expected)}")
        mism += abs(n - len(expected))

    print(f"AGENT_ESTOP oracle: {n} cases, {mism} mismatches")
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
