"""twin for hermes_cli/cli_agent_setup_mixin.py:_display_resumed_history (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/cli_agent_setup_mixin.py"
    PY_FN = "_display_resumed_history"
    C_FN = "hermes_cli_cli_agent_setup_mix_u_display_resumed_history"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
