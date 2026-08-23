"""twin for slash_access_can_run_ladder.

Traces LIVE gateway/slash_access.py::SlashAccessPolicy with the same rung
order as the C harness. The LIVE module is imported for real: lists are
built through _coerce_id_list / _coerce_command_list, the admin decision is
SlashAccessPolicy.is_admin, and the always-allowed / user-allowlist checks
run against the LIVE frozensets.
Probes: "enabled|admin_ids|allowed_cmds|user|cmd".
"""
import os
import sys
import importlib.util as ilu
import importlib

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_slash_access",
    os.path.join(DEV, "gateway", "slash_access.py"))
_sa = importlib.util.module_from_spec(_spec)
sys.modules["wc_live_slash_access"] = _sa
_spec.loader.exec_module(_sa)


def can_run_trace(enabled, admins_raw, allowed_raw, user_id, cmd):
    c = Cup("slash_policy_can_run")
    policy = _sa.SlashAccessPolicy(
        enabled=bool(enabled),
        admin_user_ids=_sa._coerce_id_list(admins_raw or None),
        user_allowed_commands=_sa._coerce_command_list(allowed_raw or None),
    )

    if not enabled:                                     # B1
        c.branch(True)
        result = True
        c.valstr("True")
        c.finalize()
        return result, c
    c.branch(False)
    if policy.is_admin(user_id):                        # B2
        c.branch(True)
        result = True
    else:
        c.branch(False)
        if not cmd:                                     # B3
            c.branch(True)
            result = False
        else:
            c.branch(False)
            canonical = cmd.strip().lstrip("/").lower()
            if canonical in _sa._ALWAYS_ALLOWED_FOR_USERS:   # B4
                c.branch(True)
                result = True
            else:
                c.branch(False)
                result = canonical in policy.user_allowed_commands  # B5
    c.valstr("True" if result else "False")
    c.finalize()
    return result, c


def run(inputs):
    outs = []
    for probe in inputs:
        parts = probe.split("|")
        parts += [""] * (5 - len(parts))
        enabled = int(parts[0]) if parts[0] else 1
        outs.append(can_run_trace(enabled, parts[1], parts[2],
                                  parts[3], parts[4])[1].finalize())
    return outs
