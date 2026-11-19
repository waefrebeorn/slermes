"""twin for agent/auxiliary_client.py:_endpoint_speaks_anthropic_messages (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/auxiliary_client.py"
    PY_FN = "_endpoint_speaks_anthropic_messages"
    C_FN = "auxc_endpoint_speaks_anthropic_messages"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
