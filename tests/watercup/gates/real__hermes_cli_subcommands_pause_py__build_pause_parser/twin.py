"""twin for hermes_cli/subcommands/pause.py:build_pause_parser (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/subcommands/pause.py"
    PY_FN = "build_pause_parser"
    C_FN = "build_pause_parser"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
