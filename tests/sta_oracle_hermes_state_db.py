#!/usr/bin/env python3
"""Oracle for the DB-backed model_config methods of hermes_state.py ported to C.

Opens the seeded DB (argv[1]) with the LIVE SessionDB class and replays the
EXACT same op sequence as tests/t_port_hermes_state_db.c, emitting one JSON
object per op. The runner diffs C vs Python.

The C harness runs FIRST and its mutations are already in the DB by the time
this oracle opens it — so we must replay the identical sequence (reads + patches)
so both sides observe identical state at every step.
"""
import json
import os
import pathlib
import sys

sys.path.insert(0, "/home/wubu/.hermes/hermes-agent")
from hermes_state import SessionDB  # noqa: E402


def js(v):
    """Return v as a JSON literal string, or 'null' when v is None."""
    return json.dumps(v) if v is not None else "null"


def main():
    # The runner hands us the C-mutated FSUB copy as argv[1]. For an honest
    # diff we must start from the PRISTINE seeded DB (ORACLE_FIXDIR/seed.db),
    # copy it to a temp, and replay the identical op sequence independently —
    # otherwise C's mutations would already be visible to us and mask any
    # divergence. Falls back to argv[1] when ORACLE_FIXDIR is unset (manual run).
    pristine = os.path.join(os.environ.get("ORACLE_FIXDIR", ""), "seed.db")
    if not os.path.isfile(pristine):
        pristine = sys.argv[1]
    tmp = os.path.join(os.path.dirname(pristine) or ".", ".oracle_hsdb_tmp.db")
    import shutil
    shutil.copyfile(pristine, tmp)
    db = SessionDB(pathlib.Path(tmp), read_only=False)

    # 1-5. reads
    print('{"op":"read_model","out":%s}' % js(db.get_session_model_config_value("sess_root", "model", None)))
    print('{"op":"read_lock","out":%s}' % js(db.get_session_model_config_value("sess_root", "browser_model_lock", None)))
    print('{"op":"read_absent","out":%s}' % js(db.get_session_model_config_value("sess_root", "absent", 42)))
    print('{"op":"read_empty","out":%s}' % js(db.get_session_model_config_value("sess_empty", "model", None)))
    print('{"op":"read_ghost","out":%s}' % js(db.get_session_model_config_value("ghost", "model", None)))

    # 6. patch merge (overwrite + new key)
    db.patch_session_model_config("sess_root", {"provider": "anthropic", "newkey": "nv"})
    provider = db.get_session_model_config_value("sess_root", "provider", None)
    print('{"op":"patch_merge","ok":true,"provider":%s}' % js(provider))

    # 7. patch None removes the key
    db.patch_session_model_config("sess_root", {"keep": None})
    keep = db.get_session_model_config_value("sess_root", "keep", "MISSING")
    print('{"op":"patch_none","ok":true,"keep":%s}' % js(keep))

    # 8. patch on nonexistent session is a no-op (SessionDB returns None)
    db.patch_session_model_config("ghost", {"x": 1})
    print('{"op":"patch_missing","ok":true}')

    # 9. empty patch is a no-op
    db.patch_session_model_config("sess_root", {})
    print('{"op":"patch_empty","ok":true}')

    db.close()


if __name__ == "__main__":
    main()
