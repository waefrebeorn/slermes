"""twin for hermes_cli/projects_db.py:remove_folder (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/projects_db.py"
    PY_FN = "remove_folder"
    C_FN = "projects_db_remove_folder"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
