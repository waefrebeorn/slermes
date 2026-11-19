"""twin for agent/secret_sources/command.py:apply_command_secrets (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/secret_sources/command.py"
    PY_FN = "apply_command_secrets"
    C_FN = "cmd_apply_command_secrets"

    def norm(self, c, result):
        c.valstr(str(int(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
