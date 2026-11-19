"""twin for tools/website_policy.py:load_website_blocklist (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/website_policy.py"
    PY_FN = "load_website_blocklist"
    C_FN = "cli_tools_website_policy_load_website_blocklist"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
