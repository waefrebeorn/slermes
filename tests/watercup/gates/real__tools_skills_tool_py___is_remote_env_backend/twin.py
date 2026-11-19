"""twin for tools/skills_tool.py:_is_remote_env_backend (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/skills_tool.py"
    PY_FN = "_is_remote_env_backend"
    C_FN = "_is_remote_env_backend"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
