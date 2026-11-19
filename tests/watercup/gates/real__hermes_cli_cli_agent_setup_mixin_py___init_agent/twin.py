"""twin for hermes_cli/cli_agent_setup_mixin.py:_init_agent (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/cli_agent_setup_mixin.py"
    PY_FN = "_init_agent"
    C_FN = "_init_agent"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
