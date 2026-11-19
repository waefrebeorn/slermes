"""twin for hermes_cli/goals.py:render_subgoals_block (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/goals.py"
    PY_FN = "render_subgoals_block"
    C_FN = "render_subgoals_block"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
