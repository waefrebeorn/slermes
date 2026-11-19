"""twin for hermes_cli/plugins.py:get_slack_action_handlers (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/plugins.py"
    PY_FN = "get_slack_action_handlers"
    C_FN = "get_slack_action_handlers"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
