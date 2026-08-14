#!/usr/bin/env python3
"""Create the shared hermes_state DB-backed oracle fixture DB.

Builds a fresh sqlite store with the real Hermes schema (via SessionDB's own
migrations) and seeds sessions covering the lifecycle + model_config surface:
  - sess_root:  a session with a nested model_config (lineage markers +
                browser_model_lock + arbitrary keys)
  - sess_empty: a session whose model_config is empty/NULL
  - sess_child: a compression continuation (parent_session_id -> sess_root)
The same DB is opened by both the C harness and the Python oracle.
"""
import os
import sys
import pathlib

ROOT = "/home/wubu/.hermes/hermes-agent"
if ROOT not in sys.path:
    sys.path.insert(0, ROOT)

from hermes_state import SessionDB  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
DB = os.path.join(HERE, "seed.db")


def main():
    if os.path.exists(DB):
        os.remove(DB)
    db = SessionDB(pathlib.Path(DB), read_only=False)

    # Root session: rich nested model_config.
    db.create_session("sess_root", "cli")
    db.patch_session_model_config("sess_root", {
        "model": "gpt-4o",
        "provider": "openai",
        "_branched_from": "parent-x",
        "browser_model_lock": {
            "model": "gpt-4o",
            "provider": "openai",
            "confirmed": True,
        },
        "keep": "value",
    })

    # Empty-config session (model_config NULL).
    db.create_session("sess_empty", "gateway")

    # Child compression continuation of root (parent link via direct SQL, as
    # there is no public Python link_child method).
    db.create_session("sess_child", "cli")
    db.patch_session_model_config("sess_child", {"model": "claude-3.7"})
    db._conn.execute(
        "UPDATE sessions SET parent_session_id = ?, end_reason = ?, ended_at = ? "
        "WHERE id = ?",
        ("sess_root", "compression", 2000.0, "sess_child"),
    )
    db._conn.commit()

    db.close()
    print("seeded", DB)


if __name__ == "__main__":
    main()
