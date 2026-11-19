"""twin for hermes_cli/gateway.py:_timestamped_stderr_gateway_command (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/gateway.py"
    PY_FN = "_timestamped_stderr_gateway_command"
    C_FN = "_timestamped_stderr_gateway_command"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
