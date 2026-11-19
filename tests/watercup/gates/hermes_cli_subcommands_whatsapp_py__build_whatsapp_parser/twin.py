"""twin for hermes_cli/subcommands/whatsapp.py:build_whatsapp_parser (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/subcommands/whatsapp.py"
    PY_FN = "build_whatsapp_parser"
    C_FN = "hermes_cli_subcommands_whatsap_build_whatsapp_parser"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
