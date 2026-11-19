"""twin for hermes_cli/sessions_cmd.py:_prune_never_active_keyed (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/sessions_cmd.py"
    PY_FN = "_prune_never_active_keyed"
    C_FN = "_prune_never_active_keyed"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
