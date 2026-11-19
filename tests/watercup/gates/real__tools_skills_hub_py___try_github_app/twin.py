"""twin for tools/skills_hub.py:_try_github_app (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/skills_hub.py"
    PY_FN = "_try_github_app"
    C_FN = "_try_github_app"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
