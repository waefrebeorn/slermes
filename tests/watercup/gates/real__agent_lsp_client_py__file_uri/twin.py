"""twin for agent/lsp/client.py:file_uri (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/lsp/client.py"
    PY_FN = "file_uri"
    C_FN = "file_uri"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
