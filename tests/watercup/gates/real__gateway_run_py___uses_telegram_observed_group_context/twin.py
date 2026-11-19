"""twin for gateway/run.py:_uses_telegram_observed_group_context (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/run.py"
    PY_FN = "_uses_telegram_observed_group_context"
    C_FN = "gateway_uses_telegram_observed_group_context"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
