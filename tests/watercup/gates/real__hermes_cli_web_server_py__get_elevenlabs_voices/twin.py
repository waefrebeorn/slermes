"""twin for hermes_cli/web_server.py:get_elevenlabs_voices (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/web_server.py"
    PY_FN = "get_elevenlabs_voices"
    C_FN = "get_elevenlabs_voices"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
