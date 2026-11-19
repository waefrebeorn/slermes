"""twin for tools/env_passthrough.py:resolve_passthrough_value (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/env_passthrough.py"
    PY_FN = "resolve_passthrough_value"
    C_FN = "cli_tools_env_passthrough_resolve_passthrough_value"

    def norm(self, c, result):
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
