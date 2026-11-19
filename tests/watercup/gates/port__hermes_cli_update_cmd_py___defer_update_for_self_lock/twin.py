"""twin for hermes_cli/update_cmd.py:_defer_update_for_self_lock (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/update_cmd.py"
    PY_FN = "_defer_update_for_self_lock"
    C_FN = "_defer_update_for_self_lock"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
