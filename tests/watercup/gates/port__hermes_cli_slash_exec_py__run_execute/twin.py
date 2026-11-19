"""twin for hermes_cli/slash_exec.py:run_execute (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/slash_exec.py"
    PY_FN = "run_execute"
    C_FN = "slash_exec_run"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
