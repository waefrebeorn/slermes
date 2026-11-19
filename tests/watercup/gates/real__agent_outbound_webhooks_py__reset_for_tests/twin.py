"""twin for agent/outbound_webhooks.py:reset_for_tests (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/outbound_webhooks.py"
    PY_FN = "reset_for_tests"
    C_FN = "ow_reset_for_tests"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
