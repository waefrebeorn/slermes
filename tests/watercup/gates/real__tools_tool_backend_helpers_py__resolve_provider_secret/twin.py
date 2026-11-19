"""twin for tools/tool_backend_helpers.py:resolve_provider_secret (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/tool_backend_helpers.py"
    PY_FN = "resolve_provider_secret"
    C_FN = "resolve_provider_secret"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
