"""twin for hermes_cli/env_loader.py:_sanitize_loaded_credentials (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/env_loader.py"
    PY_FN = "_sanitize_loaded_credentials"
    C_FN = "hermes_cli_env_loader_u_sanitize_loaded_credentials"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
