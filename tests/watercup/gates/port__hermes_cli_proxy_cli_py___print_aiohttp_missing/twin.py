"""twin for hermes_cli/proxy/cli.py:_print_aiohttp_missing (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/proxy/cli.py"
    PY_FN = "_print_aiohttp_missing"
    C_FN = "hermes_cli_proxy_cli_u_print_aiohttp_missing"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
