"""twin for hermes_cli/diagnostics_upload.py:share_to_nous (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/diagnostics_upload.py"
    PY_FN = "share_to_nous"
    C_FN = "hermes_cli_diagnostics_upload_share_to_nous"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
