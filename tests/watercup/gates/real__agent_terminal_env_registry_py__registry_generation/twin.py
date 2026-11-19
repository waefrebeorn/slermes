"""twin for agent/terminal_env_registry.py:registry_generation (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/terminal_env_registry.py"
    PY_FN = "registry_generation"
    C_FN = "registry_generation"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
