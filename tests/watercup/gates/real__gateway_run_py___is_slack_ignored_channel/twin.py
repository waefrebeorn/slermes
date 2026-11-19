"""twin for gateway/run.py:_is_slack_ignored_channel (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/run.py"
    PY_FN = "_is_slack_ignored_channel"
    C_FN = "gw_is_slack_ignored_channel"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
