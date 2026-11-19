"""twin for tools/terminal_tool.py:_is_ssh_remote_tilde_cwd (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/terminal_tool.py"
    PY_FN = "_is_ssh_remote_tilde_cwd"
    C_FN = "tt_u_is_ssh_remote_tilde_cwd"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
