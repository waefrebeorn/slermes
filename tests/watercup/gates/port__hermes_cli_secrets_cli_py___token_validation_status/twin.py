"""twin for hermes_cli/secrets_cli.py:_token_validation_status (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/secrets_cli.py"
    PY_FN = "_token_validation_status"
    C_FN = "hermes_cli_secrets_cli_u_token_validation_status"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
