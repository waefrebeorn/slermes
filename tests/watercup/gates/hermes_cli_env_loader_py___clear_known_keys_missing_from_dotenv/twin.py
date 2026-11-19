"""twin for hermes_cli/env_loader.py:_clear_known_keys_missing_from_dotenv (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/env_loader.py"
    PY_FN = "_clear_known_keys_missing_from_dotenv"
    C_FN = "hermes_cli_env_loader_u_clear_known_keys_missing_from_dotenv"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
