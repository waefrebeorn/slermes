"""twin for hermes_cli/web_server.py:_require_memory_provider_ready (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/web_server.py"
    PY_FN = "_require_memory_provider_ready"
    C_FN = "_require_memory_provider_ready"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
