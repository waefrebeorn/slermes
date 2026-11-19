"""twin for hermes_cli/proxy/cli.py:cmd_proxy_list_providers (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/proxy/cli.py"
    PY_FN = "cmd_proxy_list_providers"
    C_FN = "hermes_cli_proxy_cli_cmd_proxy_list_providers"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
