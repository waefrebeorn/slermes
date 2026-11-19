"""twin for hermes_cli/pt_input_extras.py:install_ignored_terminal_sequences (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/pt_input_extras.py"
    PY_FN = "install_ignored_terminal_sequences"
    C_FN = "install_ignored_terminal_sequences"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
