"""twin for agent/model_metadata.py:_fetch_codex_oauth_context_lengths (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/model_metadata.py"
    PY_FN = "_fetch_codex_oauth_context_lengths"
    C_FN = "_fetch_codex_oauth_context_lengths"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
