"""twin for hermes_cli/session_export_md.py:_frontmatter_value (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/session_export_md.py"
    PY_FN = "_frontmatter_value"
    C_FN = "sexmd_u_frontmatter_value"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
