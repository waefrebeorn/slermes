"""twin for hermes_cli/copilot_auth.py:_gh_cli_candidates (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/copilot_auth.py"
    PY_FN = "_gh_cli_candidates"
    C_FN = "hermes_cli_copilot_auth_u_gh_cli_candidates"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
