"""twin for agent/conversation_compression.py:_compress_context_via_codex_app_server (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/conversation_compression.py"
    PY_FN = "_compress_context_via_codex_app_server"
    C_FN = "_compress_context_via_codex_app_server"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
