"""twin for hermes_cli/session_export_html.py:generate_multi_session_html_export (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/session_export_html.py"
    PY_FN = "generate_multi_session_html_export"
    C_FN = "hermes_cli_session_export_html_generate_multi_session_html_e_rt"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
