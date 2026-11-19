"""twin for tools/web_tools.py:_has_env (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/web_tools.py"
    PY_FN = "_has_env"
    C_FN = "web_has_env"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
