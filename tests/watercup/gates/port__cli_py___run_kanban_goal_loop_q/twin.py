"""twin for cli.py:_run_kanban_goal_loop_q (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cli.py"
    PY_FN = "_run_kanban_goal_loop_q"
    C_FN = "cli_run_kanban_goal_loop_q"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
