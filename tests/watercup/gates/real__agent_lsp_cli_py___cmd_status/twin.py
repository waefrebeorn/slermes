"""twin for agent/lsp/cli.py:_cmd_status (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/lsp/cli.py"
    PY_FN = "_cmd_status"
    C_FN = "_cmd_status"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
