"""twin for hermes_cli/providers.py:is_aggregator (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/providers.py"
    PY_FN = "is_aggregator"
    C_FN = "is_aggregator"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
