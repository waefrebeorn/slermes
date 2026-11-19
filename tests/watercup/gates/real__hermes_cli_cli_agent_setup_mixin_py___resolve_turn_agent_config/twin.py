"""twin for hermes_cli/cli_agent_setup_mixin.py:_resolve_turn_agent_config (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/cli_agent_setup_mixin.py"
    PY_FN = "_resolve_turn_agent_config"
    C_FN = "_resolve_turn_agent_config"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
