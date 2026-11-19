"""twin for hermes_cli/partial_compress.py:summarize_compress_preview (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/partial_compress.py"
    PY_FN = "summarize_compress_preview"
    C_FN = "hermes_cli_partial_compress_summarize_compress_preview"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
