"""twin for agent/lsp/cli.py:_cmd_install_all (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/lsp/cli.py"
    PY_FN = "_cmd_install_all"
    C_FN = "lspc2_cmd_install_all"

    def norm(self, c, result):
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
