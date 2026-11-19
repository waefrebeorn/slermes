"""twin for agent/terminal_env_provider.py:env_description (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/terminal_env_provider.py"
    PY_FN = "env_description"
    C_FN = "env_description"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
