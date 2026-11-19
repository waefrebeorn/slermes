"""twin for cli.py:_prepare_deferred_agent_startup (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cli.py"
    PY_FN = "_prepare_deferred_agent_startup"
    C_FN = "cli_prepare_deferred_agent_startup"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
