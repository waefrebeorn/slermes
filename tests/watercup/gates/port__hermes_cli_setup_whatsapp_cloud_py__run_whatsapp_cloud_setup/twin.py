"""twin for hermes_cli/setup_whatsapp_cloud.py:run_whatsapp_cloud_setup (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/setup_whatsapp_cloud.py"
    PY_FN = "run_whatsapp_cloud_setup"
    C_FN = "hermes_cli_setup_whatsapp_clou_run_whatsapp_cloud_setup"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
