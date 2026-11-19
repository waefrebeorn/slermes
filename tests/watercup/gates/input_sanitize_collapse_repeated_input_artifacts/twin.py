"""twin for hermes_cli/input_sanitize.py:collapse_repeated_input_artifacts (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/input_sanitize.py"
    PY_FN = "collapse_repeated_input_artifacts"
    C_FN = "collapse_repeated_input_artifacts"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
