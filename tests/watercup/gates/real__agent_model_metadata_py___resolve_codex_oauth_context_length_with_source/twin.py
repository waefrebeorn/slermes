"""twin for agent/model_metadata.py:_resolve_codex_oauth_context_length_with_source (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/model_metadata.py"
    PY_FN = "_resolve_codex_oauth_context_length_with_source"
    C_FN = "agent_model_metadata_u_resolve_codex_oauth_context_length_wi_ce"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
