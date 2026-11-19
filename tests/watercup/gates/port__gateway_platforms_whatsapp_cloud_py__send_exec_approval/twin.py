"""twin for gateway/platforms/whatsapp_cloud.py:send_exec_approval (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/whatsapp_cloud.py"
    PY_FN = "send_exec_approval"
    C_FN = "send_exec_approval"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
