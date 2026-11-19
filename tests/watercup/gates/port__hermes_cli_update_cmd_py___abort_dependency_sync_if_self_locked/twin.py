"""twin for hermes_cli/update_cmd.py:_abort_dependency_sync_if_self_locked (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/update_cmd.py"
    PY_FN = "_abort_dependency_sync_if_self_locked"
    C_FN = "_abort_dependency_sync_if_self_locked"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
