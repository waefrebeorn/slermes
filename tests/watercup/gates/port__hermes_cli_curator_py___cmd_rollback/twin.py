"""twin for hermes_cli/curator.py:_cmd_rollback (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/curator.py"
    PY_FN = "_cmd_rollback"
    C_FN = "cmd_rollback"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
