"""twin for gateway/run.py:_should_send_telegram_capability_hint (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/run.py"
    PY_FN = "_should_send_telegram_capability_hint"
    C_FN = "_should_send_telegram_capability_hint"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
