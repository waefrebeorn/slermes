"""twin for hermes_cli/dump.py:_count_mcp_servers (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/dump.py"
    PY_FN = "_count_mcp_servers"
    C_FN = "hermes_cli_dump_u_count_mcp_servers"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
