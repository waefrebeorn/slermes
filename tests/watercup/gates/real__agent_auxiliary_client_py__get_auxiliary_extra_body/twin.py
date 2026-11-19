"""twin for agent/auxiliary_client.py:get_auxiliary_extra_body (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/auxiliary_client.py"
    PY_FN = "get_auxiliary_extra_body"
    C_FN = "get_auxiliary_extra_body"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
