"""twin for hermes_cli/copilot_auth.py:_probe_gh_cli_token (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/copilot_auth.py"
    PY_FN = "_probe_gh_cli_token"
    C_FN = "_probe_gh_cli_token"

    def norm(self, c, result):
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
