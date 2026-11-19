"""twin for gateway/wake.py:_self_post_chat_completion (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/wake.py"
    PY_FN = "_self_post_chat_completion"
    C_FN = "gateway_wake_u_self_post_chat_completion"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
