"""twin for hermes_cli/journey.py:_resolve_charted_signal (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/journey.py"
    PY_FN = "_resolve_charted_signal"
    C_FN = "_resolve_charted_signal"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
