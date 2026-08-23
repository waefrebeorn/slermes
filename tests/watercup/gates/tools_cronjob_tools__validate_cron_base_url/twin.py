"""twin for tools_cronjob_tools__validate_cron_base_url.

Traces the LIVE tools/cronjob_tools.py::_validate_cron_base_url with the SAME
rung order as the C harness:
  B1 base_url empty / B2 provider empty / B3 provider == "custom" /
  VSTR(class in {allow, no_provider, blocked}).
The LIVE fn is called directly; its None/str result is mapped onto the class.
Inputs stay inside the agreement zone documented in gate.json.
"""
import os, sys, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_cronjob_tools", os.path.join(DEV, "tools", "cronjob_tools.py"))
_ct = ilu.module_from_spec(_spec)
sys.modules["wc_live_tools_cronjob_tools"] = _ct
try:
    _spec.loader.exec_module(_ct)
    _live_fn = _ct._validate_cron_base_url
except Exception:
    raise


def trace(tok):
    c = Cup("cronjob_validate_cron_base_url")
    if "|" in tok:
        prov, bu = tok.split("|", 1)
    else:
        prov, bu = tok, ""
    res = _live_fn(prov, bu)

    nbu = (bu or "").strip().rstrip("/")
    c.branch(not nbu)                                   # B1
    if not nbu:
        # LIVE must have returned None here
        assert res is None
        c.valstr("allow")
        return c.finalize()

    nprov = (prov or "").strip()
    c.branch(not nprov)                                 # B2
    if not nprov:
        c.valstr("no_provider")
        return c.finalize()

    c.branch(nprov.lower() == "custom")                 # B3
    if res is None:
        c.valstr("allow")
    else:
        c.valstr("blocked")
    return c.finalize()


def run(inputs):
    return [trace(t) for t in inputs]
