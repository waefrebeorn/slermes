"""twin for hermes_cli/doctor.py:collect_relay_plugin_cutover_findings (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/doctor.py"
    PY_FN = "collect_relay_plugin_cutover_findings"
    C_FN = "collect_relay_plugin_cutover_findings"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
