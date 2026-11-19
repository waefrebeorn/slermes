"""twin for hermes_cli/plugins.py:set_gateway_message_injector (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/plugins.py"
    PY_FN = "set_gateway_message_injector"
    C_FN = "set_gateway_message_injector"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
