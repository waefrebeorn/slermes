"""twin for hermes_cli/observability/relay_shared_metrics.py:start_tool_call (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/observability/relay_shared_metrics.py"
    PY_FN = "start_tool_call"
    C_FN = "start_tool_call"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
