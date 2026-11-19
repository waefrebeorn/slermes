"""twin for agent/codex_responses_adapter.py:_canonical_call_id_from_fc (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/codex_responses_adapter.py"
    PY_FN = "_canonical_call_id_from_fc"
    C_FN = "_canonical_call_id_from_fc"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
