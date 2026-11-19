"""twin for gateway/run.py:_reload_runtime_env_preserving_config_authority (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/run.py"
    PY_FN = "_reload_runtime_env_preserving_config_authority"
    C_FN = "gw_reload_runtime_env_preserving_config_authority"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
