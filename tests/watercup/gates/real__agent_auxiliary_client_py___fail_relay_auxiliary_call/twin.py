"""twin for agent/auxiliary_client.py:_fail_relay_auxiliary_call (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/auxiliary_client.py"
    PY_FN = "_fail_relay_auxiliary_call"
    C_FN = "auxc_fail_relay_auxiliary_call"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
