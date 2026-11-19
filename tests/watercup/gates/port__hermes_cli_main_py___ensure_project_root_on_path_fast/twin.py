"""twin for hermes_cli/main.py:_ensure_project_root_on_path_fast (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/main.py"
    PY_FN = "_ensure_project_root_on_path_fast"
    C_FN = "main_g_ensure_project_root_on_path_fast"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
