"""twin for agent/auxiliary_client.py:_set_relay_auxiliary_route (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/auxiliary_client.py"
    PY_FN = "_set_relay_auxiliary_route"
    C_FN = "auxc_set_relay_auxiliary_route"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
