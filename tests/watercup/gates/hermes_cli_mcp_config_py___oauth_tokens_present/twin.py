"""twin for hermes_cli/mcp_config.py:_oauth_tokens_present (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/mcp_config.py"
    PY_FN = "_oauth_tokens_present"
    C_FN = "hermes_cli_mcp_config_u_oauth_tokens_present"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
