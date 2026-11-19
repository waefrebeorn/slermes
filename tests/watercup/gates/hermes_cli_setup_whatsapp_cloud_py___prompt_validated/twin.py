"""twin for hermes_cli/setup_whatsapp_cloud.py:_prompt_validated (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/setup_whatsapp_cloud.py"
    PY_FN = "_prompt_validated"
    C_FN = "hermes_cli_setup_whatsapp_clou_u_prompt_validated"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
