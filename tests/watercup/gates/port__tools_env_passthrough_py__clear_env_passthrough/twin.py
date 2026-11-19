"""twin for tools/env_passthrough.py:clear_env_passthrough (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/env_passthrough.py"
    PY_FN = "clear_env_passthrough"
    C_FN = "allowlist"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
