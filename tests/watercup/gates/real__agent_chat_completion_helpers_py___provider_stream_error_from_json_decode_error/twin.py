"""twin for agent/chat_completion_helpers.py:_provider_stream_error_from_json_decode_error (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/chat_completion_helpers.py"
    PY_FN = "_provider_stream_error_from_json_decode_error"
    C_FN = "_provider_stream_error_from_json_decode_error"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
