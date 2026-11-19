"""twin for agent/agent_init.py:_codex_gpt55_autoraise_notice_marker (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/agent_init.py"
    PY_FN = "_codex_gpt55_autoraise_notice_marker"
    C_FN = "agent_agent_init_u_codex_gpt55_autoraise_notice_marker"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
