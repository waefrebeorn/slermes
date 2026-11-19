"""twin for gateway/relay/adapter.py:auto_thread_info_for_chat (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/relay/adapter.py"
    PY_FN = "auto_thread_info_for_chat"
    C_FN = "auto_thread_info_for_chat"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
