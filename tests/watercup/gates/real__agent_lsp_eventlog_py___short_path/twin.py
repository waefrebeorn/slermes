"""twin for agent/lsp/eventlog.py:_short_path (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/lsp/eventlog.py"
    PY_FN = "_short_path"
    C_FN = "_short_path"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
