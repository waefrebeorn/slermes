#!/usr/bin/env python3
"""Differential oracle for plugins/web/ddgs/_search_worker.py
(gate w24_searchwork_58ae49).

The live worker is imported as a package module and its ``main()`` is actually
run: one JSON request is fed through ``sys.stdin``, the envelope is captured
off ``sys.stdout`` and the returned exit code is kept.  Three seams are
supplied by the oracle:

  * the provider call (``_run_ddgs_search``) answers the deterministic echo of
    its arguments, so the envelope shape is still live code;
  * ``HERMES_DDGS_ALLOW_TEST_HOOKS`` is set per case, so the env gate is live;
  * only the "empty" hook is exercised -- "sleep"/"gil" exist to prove a hung
    child is killable and would block for 30 seconds by design.

A case whose ``int(safe_limit)`` raises propagates out of main() (that line
sits outside its try/except); that is the error seam and the C side must
answer null.
"""
import contextlib
import io
import json
import os
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w24_searchwork_58ae49"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from plugins.web.ddgs import _search_worker as live  # noqa: E402
from plugins.web.ddgs import provider as ddgs_provider  # noqa: E402

WANTED = {"handle", "normalize", "constants"}
ENV = "HERMES_DDGS_ALLOW_TEST_HOOKS"


def _echo_provider(query, safe_limit):
    """The provider seam: deterministic, and it echoes its arguments so the
    envelope proves the normalisation reached it."""
    return [{"query": query, "limit": safe_limit}]


def run_main(r):
    hooks = "1" if r.get("hooks_enabled") else None
    request = {k: v for k, v in r.items() if k not in ("func", "case", "out", "hooks_enabled")}
    saved_stdin = sys.stdin
    saved_env = os.environ.get(ENV)
    saved_provider = ddgs_provider._run_ddgs_search
    buf = io.StringIO()
    ddgs_provider._run_ddgs_search = _echo_provider
    if hooks is None:
        os.environ.pop(ENV, None)
    else:
        os.environ[ENV] = hooks
    sys.stdin = io.StringIO(json.dumps(request))
    try:
        with contextlib.redirect_stdout(buf):
            code = live.main()
    except Exception:  # noqa: BLE001 - outside main's try/except: error seam
        return None
    finally:
        sys.stdin = saved_stdin
        ddgs_provider._run_ddgs_search = saved_provider
        if saved_env is None:
            os.environ.pop(ENV, None)
        else:
            os.environ[ENV] = saved_env
    text = buf.getvalue().strip()
    try:
        envelope = json.loads(text)
    except ValueError:
        return None
    return {"code": code, "envelope": envelope}


def normalize(r):
    """The same two expressions main() evaluates, kept in step with the port."""
    query = str(r.get("query") or "")
    safe_limit = max(1, int(r.get("safe_limit") or 1))
    return {"query": query, "safe_limit": safe_limit}


def constants():
    return {
        "test_hooks": sorted(live._TEST_HOOKS),
        "hooks_env": ENV,
        "texts": {
            "refused": "test_hook refused (hooks not enabled)",
            "unknown_prefix": "unknown test_hook: ",
            "hang_suffix": " hook returned unexpectedly",
            "invalid_prefix": "invalid request: ",
        },
    }


def expected(r):
    f = r.get("func")
    try:
        if f == "handle":
            return run_main(r)
        if f == "normalize":
            return normalize(r)
        if f == "constants":
            return constants()
    except Exception:  # noqa: BLE001 - the error seam: C must answer null
        return None
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
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
