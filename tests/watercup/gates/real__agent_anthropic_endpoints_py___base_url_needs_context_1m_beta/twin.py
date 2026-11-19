"""twin for agent/anthropic_endpoints.py:_base_url_needs_context_1m_beta (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/anthropic_endpoints.py"
    PY_FN = "_base_url_needs_context_1m_beta"
    C_FN = "_base_url_needs_context_1m_beta"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
