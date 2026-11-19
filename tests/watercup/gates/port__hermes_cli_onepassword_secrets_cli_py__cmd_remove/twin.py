"""twin for hermes_cli/onepassword_secrets_cli.py:cmd_remove (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/onepassword_secrets_cli.py"
    PY_FN = "cmd_remove"
    C_FN = "hermes_cli_onepassword_secrets_cmd_remove"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
