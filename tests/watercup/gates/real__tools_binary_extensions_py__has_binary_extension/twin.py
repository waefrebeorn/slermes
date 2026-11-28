"""twin for tools/binary_extensions.py:has_binary_extension."""
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "tools/binary_extensions.py"
    PY_FN = "has_binary_extension"
    C_FN = "tools_binary_extensions_has_binary_extension"
    def norm(self, c, result):
        c.valstr("1" if result else "0")
_t = _Twin(); trace = _t.trace; run = _t.run
