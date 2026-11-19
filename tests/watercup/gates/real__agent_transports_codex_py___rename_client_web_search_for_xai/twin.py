"""twin for agent/transports/codex.py:_rename_client_web_search_for_xai (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/transports/codex.py"
    PY_FN = "_rename_client_web_search_for_xai"
    C_FN = "_rename_client_web_search_for_xai"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
