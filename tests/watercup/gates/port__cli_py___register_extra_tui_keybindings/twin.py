"""twin for cli.py:_register_extra_tui_keybindings (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cli.py"
    PY_FN = "_register_extra_tui_keybindings"
    C_FN = "hcli_register_extra_tui_keybindings"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
