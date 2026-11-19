"""twin for agent/redact.py:_has_http_method_substring (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/redact.py"
    PY_FN = "_has_http_method_substring"
    C_FN = "red_has_http_method_substring"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
