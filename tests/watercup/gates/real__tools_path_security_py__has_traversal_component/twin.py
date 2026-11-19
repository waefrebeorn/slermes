"""twin for tools/path_security.py:has_traversal_component (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/path_security.py"
    PY_FN = "has_traversal_component"
    C_FN = "cli_tools_path_security_has_traversal_component"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
