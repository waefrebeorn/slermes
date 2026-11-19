"""twin for agent/auxiliary_client.py:_acquire_async_aux_semaphore (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/auxiliary_client.py"
    PY_FN = "_acquire_async_aux_semaphore"
    C_FN = "_acquire_async_aux_semaphore"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
