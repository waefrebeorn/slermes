"""twin for hermes_cli/local_runtime/supervisor.py:_reap_orphaned_children (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/local_runtime/supervisor.py"
    PY_FN = "_reap_orphaned_children"
    C_FN = "_reap_orphaned_children"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
