"""twin for hermes_cli/kanban_diagnostics.py:_rule_repeated_failures (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/kanban_diagnostics.py"
    PY_FN = "_rule_repeated_failures"
    C_FN = "hermes_cli_kanban_diagnostics_u_rule_repeated_failures"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
