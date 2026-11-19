"""twin for agent/codex_runtime.py:_record_codex_app_server_compaction (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/codex_runtime.py"
    PY_FN = "_record_codex_app_server_compaction"
    C_FN = "codex_session_bind_compression_vtable"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
