"""twin for tools/tts_text_normalize.py:flatten_newlines_for_payload (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/tts_text_normalize.py"
    PY_FN = "flatten_newlines_for_payload"
    C_FN = "flatten_newlines_for_payload"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
