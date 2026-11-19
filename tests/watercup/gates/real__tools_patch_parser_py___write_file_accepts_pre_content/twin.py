"""twin for tools/patch_parser.py:_write_file_accepts_pre_content (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/patch_parser.py"
    PY_FN = "_write_file_accepts_pre_content"
    C_FN = "false"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
