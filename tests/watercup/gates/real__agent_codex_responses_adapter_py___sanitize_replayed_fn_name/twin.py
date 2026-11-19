"""twin for agent/codex_responses_adapter.py:_sanitize_replayed_fn_name (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/codex_responses_adapter.py"
    PY_FN = "_sanitize_replayed_fn_name"
    C_FN = "_sanitize_replayed_fn_name"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
