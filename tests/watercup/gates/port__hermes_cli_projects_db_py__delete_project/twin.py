"""twin for hermes_cli/projects_db.py:delete_project (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/projects_db.py"
    PY_FN = "delete_project"
    C_FN = "projects_db_delete_project"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
