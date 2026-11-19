"""twin for agent/terminal_env_provider.py:cache_path_base (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/terminal_env_provider.py"
    PY_FN = "cache_path_base"
    C_FN = "cache_path_base"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
