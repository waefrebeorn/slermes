"""twin for gateway/relay/adapter.py:_resolve_reply_to_for_send (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/relay/adapter.py"
    PY_FN = "_resolve_reply_to_for_send"
    C_FN = "_resolve_reply_to_for_send"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
