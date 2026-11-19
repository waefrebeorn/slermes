"""twin for tools/env_passthrough.py:_load_config_passthrough (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/env_passthrough.py"
    PY_FN = "_load_config_passthrough"
    C_FN = "_load_config_passthrough"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
