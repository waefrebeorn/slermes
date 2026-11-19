"""twin for tools/shell_heredoc.py:_mask_simple_quotes (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/shell_heredoc.py"
    PY_FN = "_mask_simple_quotes"
    C_FN = "_mask_simple_quotes"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
