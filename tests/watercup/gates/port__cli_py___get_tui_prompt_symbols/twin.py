"""twin for cli.py:_get_tui_prompt_symbols (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cli.py"
    PY_FN = "_get_tui_prompt_symbols"
    C_FN = "_get_tui_prompt_symbols"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
