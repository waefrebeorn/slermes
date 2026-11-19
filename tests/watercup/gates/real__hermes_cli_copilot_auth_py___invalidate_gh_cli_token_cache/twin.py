"""twin for hermes_cli/copilot_auth.py:_invalidate_gh_cli_token_cache (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/copilot_auth.py"
    PY_FN = "_invalidate_gh_cli_token_cache"
    C_FN = "_invalidate_gh_cli_token_cache"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
