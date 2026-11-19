"""twin for hermes_cli/update_cmd.py:_reject_unsafe_stash_restore (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/update_cmd.py"
    PY_FN = "_reject_unsafe_stash_restore"
    C_FN = "_reject_unsafe_stash_restore"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
