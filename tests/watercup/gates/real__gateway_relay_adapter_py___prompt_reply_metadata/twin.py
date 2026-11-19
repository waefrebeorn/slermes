"""twin for gateway/relay/adapter.py:_prompt_reply_metadata (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/relay/adapter.py"
    PY_FN = "_prompt_reply_metadata"
    C_FN = "_prompt_reply_metadata"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
