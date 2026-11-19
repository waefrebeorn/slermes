"""twin for tools/read_extract.py:extract_document_text (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/read_extract.py"
    PY_FN = "extract_document_text"
    C_FN = "extract_document_text"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
