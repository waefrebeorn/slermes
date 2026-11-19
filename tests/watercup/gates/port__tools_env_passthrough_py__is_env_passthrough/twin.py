"""twin for tools/env_passthrough.py:is_env_passthrough (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/env_passthrough.py"
    PY_FN = "is_env_passthrough"
    C_FN = "cli_tools_env_passthrough_is_env_passthrough"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
