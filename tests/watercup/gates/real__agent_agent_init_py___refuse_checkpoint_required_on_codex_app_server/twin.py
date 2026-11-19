"""twin for agent/agent_init.py:_refuse_checkpoint_required_on_codex_app_server (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/agent_init.py"
    PY_FN = "_refuse_checkpoint_required_on_codex_app_server"
    C_FN = "_refuse_checkpoint_required_on_codex_app_server"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
