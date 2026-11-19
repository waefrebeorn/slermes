"""twin for agent/auxiliary_client.py:aux_interrupt_protection (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/auxiliary_client.py"
    PY_FN = "aux_interrupt_protection"
    C_FN = "aux_interrupt_protection_bridge"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
