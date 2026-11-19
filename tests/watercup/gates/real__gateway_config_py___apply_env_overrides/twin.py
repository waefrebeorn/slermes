"""twin for gateway/config.py:_apply_env_overrides (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/config.py"
    PY_FN = "_apply_env_overrides"
    C_FN = "gateway_config_apply_env_overrides"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
