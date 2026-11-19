"""twin for agent/lsp/eventlog.py:log_server_error (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/lsp/eventlog.py"
    PY_FN = "log_server_error"
    C_FN = "lsp_log_server_error"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
