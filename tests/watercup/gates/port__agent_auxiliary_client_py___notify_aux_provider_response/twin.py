"""twin for agent/auxiliary_client.py:_notify_aux_provider_response (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/auxiliary_client.py"
    PY_FN = "_notify_aux_provider_response"
    C_FN = "_notify_aux_provider_response"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
