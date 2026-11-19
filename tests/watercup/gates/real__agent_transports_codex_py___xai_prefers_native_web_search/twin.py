"""twin for agent/transports/codex.py:_xai_prefers_native_web_search (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/transports/codex.py"
    PY_FN = "_xai_prefers_native_web_search"
    C_FN = "codex_xai_prefers_native_web_search"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
