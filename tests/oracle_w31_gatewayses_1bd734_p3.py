#!/usr/bin/env python3
"""Differential oracle for hermes_cli/gateway_session_store.py (gate w31_gatewayses_1bd734_p3).

The live module is imported by file path; the launch-policy shaping is called
straight off it:

  * ``_classic_only_policy`` -- main's --run-budget / -v;
  * ``_creation_policy`` -- the launch flags as ``session.create`` policy, with
    this shell's HERMES_* opt-ins read through ``env_launch_flags`` (the seam:
    the oracle sets exactly the fixture's environment and clears the five names);
  * ``_check_resume_launch``'s classic-only tail -- the exact refusal text when a
    flag differs from the session's frozen ``launch_request``.

The fixtures carry only the classic-only flags (``verbose``/``run_budget``), for
which the ``gateway_chat.check_resume_policy`` half of ``_check_resume_launch``
passes untouched; anything else is reported as a fixture error.
"""
import json
import os
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w31_gatewayses_1bd734_p3"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))


def _load_live():
    import importlib.util

    path = QUARRY / "hermes_cli" / "gateway_session_store.py"
    spec = importlib.util.spec_from_file_location("gwss_live_p3", path)
    mod = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = mod
    spec.loader.exec_module(mod)
    return mod


try:
    live = _load_live()
except Exception:  # noqa: BLE001 -- a blocked native extension in the chain
    sys.path.append(str(Path(__file__).resolve().parent))
    try:
        from hermes_seam_stubs import install as _install_seams

        _install_seams()
    except Exception:  # noqa: BLE001
        pass
    live = _load_live()

WANTED = {"classic_only_policy", "creation_policy", "resume_classic_differs"}
ENV_NAMES = ("HERMES_YOLO_MODE", "HERMES_IGNORE_RULES", "HERMES_SAFE_MODE",
             "HERMES_IGNORE_USER_CONFIG", "HERMES_ACCEPT_HOOKS")
CLASSIC_ONLY = ("verbose", "run_budget")


def classic_only_policy(r):
    return live._classic_only_policy(r.get("launch") or {})


def creation_policy(r):
    env = r.get("env") or {}
    saved = {name: os.environ.get(name) for name in ENV_NAMES}
    try:
        for name in ENV_NAMES:
            os.environ.pop(name, None)
        for name, value in env.items():
            os.environ[name] = str(value)
        return live._creation_policy(r.get("launch") or {})
    finally:
        for name, value in saved.items():
            if value is None:
                os.environ.pop(name, None)
            else:
                os.environ[name] = value


def resume_classic_differs(r):
    launch = r.get("launch") or {}
    extra = [k for k in launch if k not in CLASSIC_ONLY]
    if extra:
        raise AssertionError("fixture carries non-classic launch fields: %r" % (extra,))
    try:
        live._check_resume_launch(launch, r.get("snapshot") or {})
    except Exception as exc:  # noqa: BLE001 -- the refusal is the answer
        return {"error": str(exc)}
    return {"error": None}


def expected(r):
    f = r.get("func")
    if f == "classic_only_policy":
        return classic_only_policy(r)
    if f == "creation_policy":
        return creation_policy(r)
    if f == "resume_classic_differs":
        return resume_classic_differs(r)
    raise AssertionError("unknown func %r" % (f,))


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in WANTED:
            continue
        cases += 1
        try:
            py = expected(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r" % (r.get("func"), r.get("case"), py, got),
                  file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if (mism or cases == 0) else 0)


if __name__ == "__main__":
    main()
