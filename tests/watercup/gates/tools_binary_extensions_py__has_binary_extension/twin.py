"""twin for tools/binary_extensions.py:has_binary_extension (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/binary_extensions.py"
    PY_FN = "has_binary_extension"
    C_FN = "tools_binary_extensions_has_binary_extension"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
