"""twin for hermes_cli/subcommands/dashboard.py:_add_server_runtime_args (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/subcommands/dashboard.py"
    PY_FN = "_add_server_runtime_args"
    C_FN = "hermes_cli_subcommands_dashboa_u_add_server_runtime_args"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
