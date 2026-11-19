"""twin for tools/browser_use_cli.py:_blocked_url_in_code (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/browser_use_cli.py"
    PY_FN = "_blocked_url_in_code"
    C_FN = "_blocked_url_in_code"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
