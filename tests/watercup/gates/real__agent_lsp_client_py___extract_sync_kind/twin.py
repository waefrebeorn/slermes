"""twin for agent/lsp/client.py:_extract_sync_kind (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/lsp/client.py"
    PY_FN = "_extract_sync_kind"
    C_FN = "lspc_extract_sync_kind"

    def norm(self, c, result):
        c.valstr(str(int(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
