"""twin for hermes_cli/subcommands/peer.py:_peer_run_durability (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/subcommands/peer.py"
    PY_FN = "_peer_run_durability"
    C_FN = "_peer_run_durability"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
