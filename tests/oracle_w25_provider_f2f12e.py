#!/usr/bin/env python3
"""Differential oracle for plugins/web/parallel/provider.py (gate w25_provider_f2f12e).

The live provider is driven through its own ``search()`` with the two SDK/keyless
collaborators replaced by fixed inputs:

  * ``_get_sync_client()`` returns a recorder whose ``beta.search(**kwargs)``
    keeps the exact call and answers with the case's ``results`` list;
  * ``use_keyless`` is forced False (the paid path);
  * ``plugins.web._common._interrupted`` is forced False.

``search_plan`` is then the recorded ``beta.search`` kwargs, ``search_hits`` is
the live ``search()`` return value (``search_ok([web_hit(...)])``), and
``resolve_search_mode`` is the module's own resolver under the case's
``PARALLEL_SEARCH_MODE``.
"""
import json
import os
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w25_provider_f2f12e"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from plugins.web import _common  # noqa: E402
from plugins.web.parallel import provider as live  # noqa: E402

WANTED = {"resolve_search_mode", "search_plan", "search_hits", "constants"}
# The known modes _resolve_search_mode narrows to (inlined set in the source).
MODES = ("agentic", "fast", "one-shot")


class _Result:
    def __init__(self, data):
        self.url = data.get("url") if isinstance(data, dict) else None
        self.title = data.get("title") if isinstance(data, dict) else None
        self.excerpts = data.get("excerpts") if isinstance(data, dict) else None


class _Response:
    def __init__(self, results):
        self.results = results


class _Beta:
    def __init__(self):
        self.calls = []
        self.results = []

    def search(self, **kwargs):
        self.calls.append(dict(kwargs))
        return _Response([_Result(r) for r in self.results])


class _Client:
    def __init__(self, beta):
        self.beta = beta


BETA = _Beta()
_common._interrupted = lambda: False
live.use_keyless = lambda *a, **k: False
live._get_sync_client = lambda: _Client(BETA)


def _apply_mode(req):
    mode = req.get("mode_env")
    if mode is None:
        os.environ.pop("PARALLEL_SEARCH_MODE", None)
    else:
        os.environ["PARALLEL_SEARCH_MODE"] = str(mode)


def _drive(req):
    """Run the live search() for this case; returns (plan_kwargs, result)."""
    _apply_mode(req)
    raw = req.get("results")
    BETA.calls = []
    BETA.results = raw if isinstance(raw, list) else []
    limit = req.get("limit", 5)
    if isinstance(limit, bool) or not isinstance(limit, (int, float)):
        limit = 5
    result = live.ParallelWebSearchProvider().search(req.get("query", ""), limit)
    return BETA.calls[-1], result


def resolve_search_mode(req):
    _apply_mode(req)
    return live._resolve_search_mode()


def search_plan(req):
    plan, _ = _drive(req)
    return {k: plan[k] for k in ("mode", "max_results", "search_queries", "objective")}


def search_hits(req):
    _, result = _drive(req)
    return result


def constants():
    cls = live.ParallelWebSearchProvider
    return {
        "name": cls.NAME,
        "display_name": cls.DISPLAY_NAME,
        "key_env": cls.KEY_ENV,
        "extract": cls.EXTRACT,
        "keyless": cls.KEYLESS,
        "search_limit_cap": _common.SEARCH_LIMIT_CAP,
        "missing_key": live._MISSING_KEY,
        "search_modes": sorted(MODES),
    }


def expected(r):
    f = r.get("func")
    if f == "resolve_search_mode":
        return resolve_search_mode(r)
    if f == "search_plan":
        return search_plan(r)
    if f == "search_hits":
        return search_hits(r)
    if f == "constants":
        return constants()
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
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r" % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
