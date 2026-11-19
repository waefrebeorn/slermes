"""twin for hermes_cli/memory_setup.py:cmd_status (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/memory_setup.py"
    PY_FN = "cmd_status"
    C_FN = "cli_hermes_cli_memory_setup_cmd_status"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
