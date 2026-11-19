"""twin for agent/redact.py:is_env_dump_command (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/redact.py"
    PY_FN = "is_env_dump_command"
    C_FN = "agent_redact_is_env_dump_command"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
