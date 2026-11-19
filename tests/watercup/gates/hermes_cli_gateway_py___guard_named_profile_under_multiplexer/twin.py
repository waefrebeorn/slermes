"""twin for hermes_cli/gateway.py:_guard_named_profile_under_multiplexer (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/gateway.py"
    PY_FN = "_guard_named_profile_under_multiplexer"
    C_FN = "gwc2_guard_named_profile_under_multiplexer"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
