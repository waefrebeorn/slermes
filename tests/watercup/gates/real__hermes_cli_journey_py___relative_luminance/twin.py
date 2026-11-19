"""twin for hermes_cli/journey.py:_relative_luminance (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/journey.py"
    PY_FN = "_relative_luminance"
    C_FN = "_relative_luminance"

    def norm(self, c, result):
        c.valstr("%.17g" % float(result))


_t = _Twin()
trace = _t.trace
run = _t.run
