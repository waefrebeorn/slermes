"""twin: is_pdf_path(path) -> bool."""
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "tools/binary_extensions.py"; PY_FN = "is_pdf_path"
    C_FN = "tools_binary_extensions_u_is_pdf_path"
    def norm(self, c, result):
        c.valstr("1" if result else "0")
_t = _Twin(); trace = _t.trace; run = _t.run
