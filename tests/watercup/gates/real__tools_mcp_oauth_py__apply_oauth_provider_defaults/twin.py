"""twin for tools/mcp_oauth.py:apply_oauth_provider_defaults (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/mcp_oauth.py"
    PY_FN = "apply_oauth_provider_defaults"
    C_FN = "apply_oauth_provider_defaults"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
