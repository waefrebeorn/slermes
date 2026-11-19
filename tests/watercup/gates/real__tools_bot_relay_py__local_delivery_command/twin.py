"""twin for tools/bot_relay.py:local_delivery_command (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/bot_relay.py"
    PY_FN = "local_delivery_command"
    C_FN = "local_delivery_command"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
