"""twin for tools/environments/ssh.py:_ensure_remote_dirs (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/environments/ssh.py"
    PY_FN = "_ensure_remote_dirs"
    C_FN = "cli_tools_environments_ssh__ensure_remote_dirs"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
