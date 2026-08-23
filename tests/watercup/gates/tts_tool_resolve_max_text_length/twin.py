"""twin for tts_tool_resolve_max_text_length.

Imports LIVE tools/tts_tool.py::_resolve_max_text_length and traces the SAME
override-or-fallback rung ladder as the C port (tts_resolve_max_text_length,
src/tools/tts_tool_ports.c). Providers are restricted to names absent from
LIVE's PROVIDER_MAX_TEXT_LENGTH table and the ElevenLabs model table so the
simplified C rung order is the agreed contract (documented divergence beyond).
Input encoding: provider\x1foverride ('-' absent, 'b' bool, else integer text).
"""
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
import importlib.util as ilu
spec = ilu.spec_from_file_location("wc_tts", os.path.join(DEV, "tools", "tts_tool.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_tts"] = mod
spec.loader.exec_module(mod)

from watercup_py import Cup

def run(inputs):
    cups = []
    for raw in inputs:
        prov_s, _, ov = raw.partition("\x1f")
        prov = prov_s
        res = 4000
        c = Cup("resolve_max_text_length")
        if not prov:
            c.branch(True)
            res = 4000
        else:
            c.branch(False)
            have_ov = ov != "-"
            if have_ov:
                c.branch(True)
                if ov == "b":
                    c.branch(True)          # bool override never wins
                else:
                    c.branch(False)
                    try:
                        v = int(ov)
                    except ValueError:
                        v = -1
                    if v > 0:
                        c.branch(True); res = v
                    else:
                        c.branch(False)
            else:
                c.branch(False)
        if prov and ov != "-" and ov != "b" and ov.lstrip("-").isdigit() and int(ov) > 0:
            cfg = {prov.lower().strip(): {"max_text_length": int(ov)}}
        else:
            cfg = None
        live = mod._resolve_max_text_length(prov, cfg)
        assert res == live, f"LIVE/C diverge on {raw!r}: {live} vs {res}"
        c.valstr(str(res))
        cups.append(c.finalize())
    return cups
