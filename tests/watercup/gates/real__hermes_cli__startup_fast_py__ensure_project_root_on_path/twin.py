"""twin for hermes_cli/_startup_fast.py:ensure_project_root_on_path (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/_startup_fast.py"
    PY_FN = "ensure_project_root_on_path"
    C_FN = "sf_ensure_project_root_on_path"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
