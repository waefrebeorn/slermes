"""twin for gateway/relay/adapter.py:_effective_reply_in_thread (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/relay/adapter.py"
    PY_FN = "_effective_reply_in_thread"
    C_FN = "relay_helper_effective_reply_in_thread"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
