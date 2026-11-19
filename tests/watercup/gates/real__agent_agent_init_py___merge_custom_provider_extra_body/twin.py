"""twin for agent/agent_init.py:_merge_custom_provider_extra_body (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/agent_init.py"
    PY_FN = "_merge_custom_provider_extra_body"
    C_FN = "_merge_custom_provider_extra_body"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
