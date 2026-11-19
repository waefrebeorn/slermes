"""twin for hermes_cli/diagnostics_upload.py:request_upload_url (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/diagnostics_upload.py"
    PY_FN = "request_upload_url"
    C_FN = "hermes_cli_diagnostics_upload_request_upload_url"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
