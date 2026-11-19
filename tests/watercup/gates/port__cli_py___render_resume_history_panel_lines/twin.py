"""twin for cli.py:_render_resume_history_panel_lines (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cli.py"
    PY_FN = "_render_resume_history_panel_lines"
    C_FN = "_render_resume_history_panel_lines"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
