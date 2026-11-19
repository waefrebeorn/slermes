"""twin for gateway/platforms/weixin.py:_should_split_short_chat_block_for_weixin (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/weixin.py"
    PY_FN = "_should_split_short_chat_block_for_weixin"
    C_FN = "should_split_short_chat_block_for_weixin"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
