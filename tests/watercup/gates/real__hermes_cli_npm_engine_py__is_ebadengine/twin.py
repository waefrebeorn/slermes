"""twin for hermes_cli/npm_engine.py:is_ebadengine (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/npm_engine.py"
    PY_FN = "is_ebadengine"
    C_FN = "ne_is_ebadengine"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
