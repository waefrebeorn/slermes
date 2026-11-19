"""twin for hermes_cli/secret_prompt.py:_stream_is_tty (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/secret_prompt.py"
    PY_FN = "_stream_is_tty"
    C_FN = "hermes_cli_secret_prompt_u_stream_is_tty"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
