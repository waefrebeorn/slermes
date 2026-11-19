"""twin for agent/redact.py:_redact_url_query_params (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/redact.py"
    PY_FN = "_redact_url_query_params"
    C_FN = "_redact_url_query_params"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
