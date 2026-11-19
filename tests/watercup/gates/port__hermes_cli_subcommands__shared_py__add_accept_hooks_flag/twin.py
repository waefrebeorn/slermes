"""twin for hermes_cli/subcommands/_shared.py:add_accept_hooks_flag (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/subcommands/_shared.py"
    PY_FN = "add_accept_hooks_flag"
    C_FN = "hermes_cli_subcommands__shared_add_accept_hooks_flag"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
