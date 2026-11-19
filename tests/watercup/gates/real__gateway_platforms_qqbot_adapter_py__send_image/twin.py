"""twin for gateway/platforms/qqbot/adapter.py:send_image (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/qqbot/adapter.py"
    PY_FN = "send_image"
    C_FN = "qqbot_send_image"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
