"""twin for hermes_cli/web_server.py:_split_text_for_speak_stream (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/web_server.py"
    PY_FN = "_split_text_for_speak_stream"
    C_FN = "_split_text_for_speak_stream"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
