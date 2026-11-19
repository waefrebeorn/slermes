"""twin for agent/auxiliary_client.py:_aux_interrupt_protected (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/auxiliary_client.py"
    PY_FN = "_aux_interrupt_protected"
    C_FN = "auxc_aux_interrupt_protected"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
