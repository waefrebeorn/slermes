"""twin for hermes_cli/session_export_html.py:_escape_html (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/session_export_html.py"
    PY_FN = "_escape_html"
    C_FN = "hermes_cli_session_export_html_u_escape_html"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
