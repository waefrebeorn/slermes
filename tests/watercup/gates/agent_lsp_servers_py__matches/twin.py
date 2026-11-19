"""twin for agent/lsp/servers.py:matches (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/lsp/servers.py"
    PY_FN = "matches"
    C_FN = "lsp_server_matches"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
