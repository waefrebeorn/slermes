"""twin for hermes_cli/kanban_decompose.py:list_triage_ids (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/kanban_decompose.py"
    PY_FN = "list_triage_ids"
    C_FN = "list_triage_ids"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
