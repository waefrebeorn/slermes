"""twin for agent/lsp/install.py:hermes_lsp_bin_dir (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/lsp/install.py"
    PY_FN = "hermes_lsp_bin_dir"
    C_FN = "hermes_lsp_bin_dir"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
