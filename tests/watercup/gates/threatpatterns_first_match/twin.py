"""twin for tools/threat_patterns.py:scan_for_threats (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/threat_patterns.py"
    PY_FN = "scan_for_threats"
    C_FN = "cli_tools_threat_patterns_scan_for_threats"

    def norm(self, c, result):
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
