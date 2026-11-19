"""twin for agent/auxiliary_client.py:_reset_aux_semaphores (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/auxiliary_client.py"
    PY_FN = "_reset_aux_semaphores"
    C_FN = "auxc_reset_aux_semaphores"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
