"""twin for agent/agent_runtime_helpers.py:_apply_switched_provider_request_overrides (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/agent_runtime_helpers.py"
    PY_FN = "_apply_switched_provider_request_overrides"
    C_FN = "_apply_switched_provider_request_overrides"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
