"""twin for gateway/status.py:_gateway_command_subcommand (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/status.py"
    PY_FN = "_gateway_command_subcommand"
    C_FN = "_gateway_command_subcommand"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
