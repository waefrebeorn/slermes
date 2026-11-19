"""twin for tools/browser_lightpanda.py:cdp_url (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/browser_lightpanda.py"
    PY_FN = "cdp_url"
    C_FN = "cdp_url"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
