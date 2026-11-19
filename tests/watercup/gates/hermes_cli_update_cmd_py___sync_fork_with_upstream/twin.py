"""twin for hermes_cli/update_cmd.py:_sync_fork_with_upstream (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/update_cmd.py"
    PY_FN = "_sync_fork_with_upstream"
    C_FN = "uc_sync_fork_with_upstream"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
