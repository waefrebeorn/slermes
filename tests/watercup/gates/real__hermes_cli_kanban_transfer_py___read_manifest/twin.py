"""twin for hermes_cli/kanban_transfer.py:_read_manifest (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/kanban_transfer.py"
    PY_FN = "_read_manifest"
    C_FN = "_read_manifest"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
