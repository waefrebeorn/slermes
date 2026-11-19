"""twin for tools/schema_sanitizer.py:_rename_property_keys (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/schema_sanitizer.py"
    PY_FN = "_rename_property_keys"
    C_FN = "cli_tools_schema_sanitizer__rename_property_keys"

    def norm(self, c, result):
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
