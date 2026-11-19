"""twin for agent/codex_responses_adapter.py:_clamp_responses_call_id (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/codex_responses_adapter.py"
    PY_FN = "_clamp_responses_call_id"
    C_FN = "_clamp_responses_call_id"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
