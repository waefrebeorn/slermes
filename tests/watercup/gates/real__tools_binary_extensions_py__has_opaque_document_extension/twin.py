"""twin for tools/binary_extensions.py:has_opaque_document_extension (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/binary_extensions.py"
    PY_FN = "has_opaque_document_extension"
    C_FN = "has_opaque_document_extension"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
