"""twin for agent/secret_sources/base.py:is_valid_env_name (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/secret_sources/base.py"
    PY_FN = "is_valid_env_name"
    C_FN = "is_valid_env_name"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
