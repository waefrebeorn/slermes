"""twin for hermes_cli/route_identity.py:should_clear_context_pin (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/route_identity.py"
    PY_FN = "should_clear_context_pin"
    C_FN = "hermes_cli_route_identity_should_clear_context_pin"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
