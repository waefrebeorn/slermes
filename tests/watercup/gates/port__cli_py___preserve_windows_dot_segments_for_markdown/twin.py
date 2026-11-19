"""twin for cli.py:_preserve_windows_dot_segments_for_markdown (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cli.py"
    PY_FN = "_preserve_windows_dot_segments_for_markdown"
    C_FN = "_preserve_windows_dot_segments_for_markdown"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
