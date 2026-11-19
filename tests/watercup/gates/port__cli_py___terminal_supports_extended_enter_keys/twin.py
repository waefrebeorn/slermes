"""twin for cli.py:_terminal_supports_extended_enter_keys (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cli.py"
    PY_FN = "_terminal_supports_extended_enter_keys"
    C_FN = "cli_terminal_supports_extended_enter_keys"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
