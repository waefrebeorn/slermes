"""twin for gateway/run.py:_cancel_process_completion_batch_tasks (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/run.py"
    PY_FN = "_cancel_process_completion_batch_tasks"
    C_FN = "_cancel_process_completion_batch_tasks"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
