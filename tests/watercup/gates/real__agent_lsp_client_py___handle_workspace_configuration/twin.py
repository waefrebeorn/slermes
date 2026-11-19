"""twin for agent/lsp/client.py:_handle_workspace_configuration (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/lsp/client.py"
    PY_FN = "_handle_workspace_configuration"
    C_FN = "_handle_workspace_configuration"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
