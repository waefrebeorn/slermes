"""twin for hermes_cli/gateway.py:_probe_launchd_domain_for_label (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/gateway.py"
    PY_FN = "_probe_launchd_domain_for_label"
    C_FN = "_probe_launchd_domain_for_label"

    def norm(self, c, result):
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
