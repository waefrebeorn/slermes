"""twin for hermes_cli/kanban.py:_is_delegated_child_cli_mutation (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/kanban.py"
    PY_FN = "_is_delegated_child_cli_mutation"
    C_FN = "_is_delegated_child_cli_mutation"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
