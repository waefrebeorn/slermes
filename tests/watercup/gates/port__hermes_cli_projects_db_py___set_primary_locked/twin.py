"""twin for hermes_cli/projects_db.py:_set_primary_locked (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/projects_db.py"
    PY_FN = "_set_primary_locked"
    C_FN = "set_primary_locked"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
