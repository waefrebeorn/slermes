"""twin for agent/auxiliary_client.py:auxiliary_max_tokens_param (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/auxiliary_client.py"
    PY_FN = "auxiliary_max_tokens_param"
    C_FN = "auxiliary_max_tokens_param"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
