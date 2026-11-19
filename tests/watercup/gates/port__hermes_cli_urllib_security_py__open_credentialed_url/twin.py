"""twin for hermes_cli/urllib_security.py:open_credentialed_url (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/urllib_security.py"
    PY_FN = "open_credentialed_url"
    C_FN = "hermes_cli_urllib_security_open_credentialed_url"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
