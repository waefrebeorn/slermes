"""twin for agent/outbound_webhooks.py:_parse_single_target (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/outbound_webhooks.py"
    PY_FN = "_parse_single_target"
    C_FN = "_parse_single_target"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
