"""twin for agent/agent_runtime_helpers.py:dump_api_request_debug (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/agent_runtime_helpers.py"
    PY_FN = "dump_api_request_debug"
    C_FN = "dump_api_request_debug"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
