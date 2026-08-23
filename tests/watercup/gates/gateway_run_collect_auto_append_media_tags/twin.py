"""twin for gateway_run_collect_auto_append_media_tags.

Traces the LIVE gateway/run.py::_collect_auto_append_media_tags over the C
port's outcome-level rung ladder:

  B1 messages not a list / per-tag V events / B2 voice directive / V bool.

The live fn runs with history_offset=0 and an empty dedup set; inputs are
restricted to producer-allowlisted tools (assistant tool_calls declaring
text_to_speech) with extension-anchored absolute MEDIA paths, where the C
port's naive token scan agrees with LIVE's allowlist+regex pipeline.
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
    live_collect = _grun._collect_auto_append_media_tags
except Exception:  # pragma: no cover
    live_collect = None


def trace(messages_json):
    c = Cup("collect_auto_append_media_tags")
    try:
        messages = json.loads(messages_json)
    except Exception:
        messages = None
    if not isinstance(messages, list):                           # B1
        c.branch(True)
        c.branch(False)
        c.valstr("False")
        return c.finalize()
    c.branch(False)
    tags, voice = live_collect(messages, 0, set()) if live_collect else ([], False)
    for t in tags:
        c.valstr(t)
    c.branch(bool(voice))                                        # B2
    c.valstr("True" if voice else "False")
    return c.finalize()


def run(inputs):
    return [trace(s) for s in inputs]
