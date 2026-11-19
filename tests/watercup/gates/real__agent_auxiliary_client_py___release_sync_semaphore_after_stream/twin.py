"""twin for agent/auxiliary_client.py:_release_sync_semaphore_after_stream (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/auxiliary_client.py"
    PY_FN = "_release_sync_semaphore_after_stream"
    C_FN = "auxc_release_sync_semaphore_after_stream"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
