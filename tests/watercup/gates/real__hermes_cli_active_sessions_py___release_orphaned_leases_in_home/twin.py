"""twin for hermes_cli/active_sessions.py:_release_orphaned_leases_in_home (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/active_sessions.py"
    PY_FN = "_release_orphaned_leases_in_home"
    C_FN = "_release_orphaned_leases_in_home"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
