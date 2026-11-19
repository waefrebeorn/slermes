"""twin for hermes_cli/projects_cmd.py:_print_project (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/projects_cmd.py"
    PY_FN = "_print_project"
    C_FN = "hermes_cli_projects_cmd_u_print_project"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
