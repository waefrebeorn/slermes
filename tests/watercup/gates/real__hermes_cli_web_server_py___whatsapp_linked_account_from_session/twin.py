"""twin for hermes_cli/web_server.py:_whatsapp_linked_account_from_session (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/web_server.py"
    PY_FN = "_whatsapp_linked_account_from_session"
    C_FN = "_whatsapp_linked_account_from_session"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
