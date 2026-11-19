"""twin for hermes_cli/clipboard.py:is_remote_shell_session (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/clipboard.py"
    PY_FN = "is_remote_shell_session"
    C_FN = "clipboard_is_remote_shell_session"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
