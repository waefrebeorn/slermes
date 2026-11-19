"""twin for hermes_cli/local_runtime/presets.py:read_preset_decisions (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/local_runtime/presets.py"
    PY_FN = "read_preset_decisions"
    C_FN = "read_preset_decisions"

    def norm(self, c, result):
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
