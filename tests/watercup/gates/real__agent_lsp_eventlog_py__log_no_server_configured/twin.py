"""twin for agent/lsp/eventlog.py:log_no_server_configured (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/lsp/eventlog.py"
    PY_FN = "log_no_server_configured"
    C_FN = "lsp_log_no_server_configured"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
