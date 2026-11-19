"""twin for agent/auxiliary_client.py:_record_relay_auxiliary_response_model (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/auxiliary_client.py"
    PY_FN = "_record_relay_auxiliary_response_model"
    C_FN = "aux_record_relay_response_model"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
