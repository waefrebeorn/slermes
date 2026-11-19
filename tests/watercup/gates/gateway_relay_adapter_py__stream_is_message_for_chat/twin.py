"""twin for gateway/relay/adapter.py:stream_is_message_for_chat (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/relay/adapter.py"
    PY_FN = "stream_is_message_for_chat"
    C_FN = "relay_adapter_stream_is_message_for_chat"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
