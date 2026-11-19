"""twin for hermes_cli/kanban_db.py:_resolve_worker_cli_toolsets (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/kanban_db.py"
    PY_FN = "_resolve_worker_cli_toolsets"
    C_FN = "_resolve_worker_cli_toolsets"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
