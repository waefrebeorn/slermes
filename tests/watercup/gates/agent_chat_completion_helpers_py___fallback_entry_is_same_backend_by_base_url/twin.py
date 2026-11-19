"""twin for agent/chat_completion_helpers.py:_fallback_entry_is_same_backend_by_base_url (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/chat_completion_helpers.py"
    PY_FN = "_fallback_entry_is_same_backend_by_base_url"
    C_FN = "agent_chat_completion_helpers_u_fallback_entry_is_same_backe_rl"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
