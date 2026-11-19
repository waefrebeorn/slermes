"""twin for hermes_cli/webhook.py:_get_webhook_config (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/webhook.py"
    PY_FN = "_get_webhook_config"
    C_FN = "hermes_cli_webhook_u_get_webhook_config"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
