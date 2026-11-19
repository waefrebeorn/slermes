"""twin for agent/lsp/client.py:_pull_document_diagnostics (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/lsp/client.py"
    PY_FN = "_pull_document_diagnostics"
    C_FN = "_pull_document_diagnostics"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
