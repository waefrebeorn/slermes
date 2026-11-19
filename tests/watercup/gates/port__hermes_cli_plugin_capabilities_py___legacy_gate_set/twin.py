"""twin for hermes_cli/plugin_capabilities.py:_legacy_gate_set (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/plugin_capabilities.py"
    PY_FN = "_legacy_gate_set"
    C_FN = "pc_legacy_gate_set"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
