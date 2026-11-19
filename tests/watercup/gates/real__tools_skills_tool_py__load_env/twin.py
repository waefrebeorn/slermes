"""twin for tools/skills_tool.py:load_env (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/skills_tool.py"
    PY_FN = "load_env"
    C_FN = "load_env"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
