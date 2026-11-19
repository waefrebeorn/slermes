"""twin for gateway/run.py:_enqueue_process_completion_notification (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/run.py"
    PY_FN = "_enqueue_process_completion_notification"
    C_FN = "_enqueue_process_completion_notification"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
