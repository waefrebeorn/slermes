"""twin for gateway_run_collect_history_media_paths.

Traces the LIVE gateway/run.py::_collect_history_media_paths over the C
port's outcome-level rung ladder:

  B1 history not a list / V per unique path (sorted canonical order) /
  final V comma-joined sorted list.

The live fn returns a set; the twin sorts it so V-event order matches the
C harness's canonicalized first-seen order. Inputs are tool/function-only
histories with plain extension-anchored MEDIA paths.
"""
import os, sys, json, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_gateway_run", os.path.join(DEV, "gateway", "run.py"))
_grun = ilu.module_from_spec(_spec)
sys.modules["wc_live_gateway_run"] = _grun
try:
    _spec.loader.exec_module(_grun)
    live_collect = _grun._collect_history_media_paths
except Exception:  # pragma: no cover
    live_collect = None


def trace(history_json):
    c = Cup("collect_history_media_paths")
    try:
        hist = json.loads(history_json)
    except Exception:
        hist = None
    if not isinstance(hist, list):                               # B1
        c.branch(True)
        c.valstr("")
        return c.finalize()
    c.branch(False)
    paths = sorted(live_collect(hist)) if live_collect else []
    joined = ",".join(paths)
    for p in paths:
        c.valstr(p)
    c.valstr(joined)
    return c.finalize()


def run(inputs):
    return [trace(s) for s in inputs]
