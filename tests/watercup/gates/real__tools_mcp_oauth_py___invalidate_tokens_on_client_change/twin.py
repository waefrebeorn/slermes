"""twin for tools/mcp_oauth.py:_invalidate_tokens_on_client_change (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/mcp_oauth.py"
    PY_FN = "_invalidate_tokens_on_client_change"
    C_FN = "_invalidate_tokens_on_client_change"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
