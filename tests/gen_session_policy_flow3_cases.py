#!/usr/bin/env python3
"""Generate tests/fixtures/session_policy_flow3.jsonl -- differential cases for
the gateway/session_policy.py third-wave FLOW port
(src/gateway/session_policy_flow3.c).

Deterministic, >=5 cases per ported func: branch boundaries, scripted
sequences, fail-closed seams (missing/empty script -> RuntimeError), real
exception kinds (Exception swallowed where live swallows; BaseException passes
through), named AuthorityRefusal refusals, tuple-path secret-map encoding, and
the documented scalar/strict-JSON/set-iteration domains. Mutable-state funcs
(bind/rebind/release_launch_secrets, _extract_config_secrets) carry
in.authority / in.value+in.private.

Run: python tests/gen_session_policy_flow3_cases.py
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "session_policy_flow3.jsonl")

CASES = []


def C(func, io=None, **inp):
    CASES.append({"func": func, "in": dict(inp, io=io if io is not None else {})})


def R(kind, msg=""):
    return {"__raise__": kind, "msg": msg}


def AR(reason, field=None, detail=None):
    d = {"__raise__": "AuthorityRefusal", "msg": reason}
    if field is not None:
        d["field"] = field
    if detail is not None:
        d["detail"] = detail
    return d


BASE_POLICY = {"source": "cli", "platform": "cli", "cwd": "/x", "model": "m",
               "toolsets": ["a"], "config_json": "{}", "request_json": "{}",
               "terminal_json": "{}"}
FULL_REQ = dict(BASE_POLICY)


def pk(*path):
    return json.dumps(list(path), separators=(",", ":"))


# -- _refuse --
C("_refuse", {}, field="api_key", detail="must be a non-empty string")
C("_refuse", {}, field="x", detail=None)
C("_refuse", {}, field=None, detail=None)
C("_refuse", {}, field="  a  b  ", detail="  c\t d  ")
C("_refuse", {}, field="x" * 100, detail="y" * 200)
C("_refuse", {}, field="a\x01b", detail="c\x02d")

# -- _policy_refused --
C("_policy_refused", {}, field="policy", detail="the stored policy is not an object")
C("_policy_refused", {}, field="cwd", detail=None)
C("_policy_refused", {}, field=None, detail=None)
C("_policy_refused", {}, field="  m  n  ", detail="  p\t q  ")
C("_policy_refused", {}, field="z" * 100, detail="w" * 200)

# -- _adopt_stored_policy --
C("_adopt_stored_policy", {}, data=None)
C("_adopt_stored_policy", {}, data={"source": "cli"})
C("_adopt_stored_policy", {"logger.warning": None}, data={"source": "cli", "bogus": "x"})
C("_adopt_stored_policy", {"logger.warning": None}, data={"zeta": 1, "alpha": 2, "source": "cli"})
C("_adopt_stored_policy", {"logger.warning": None}, data={"bogus": 1})
C("_adopt_stored_policy", {}, data={"bogus": 1})  # fail-closed logger.warning
C("_adopt_stored_policy", {"logger.warning": R("SystemExit", "bye")}, data={"bogus": 1})
C("_adopt_stored_policy", {}, data={})
C("_adopt_stored_policy", {}, data={"source": "cli", "platform": "cli"})

# -- _stored_policy --
C("_stored_policy", {}, data=None)
C("_stored_policy", {}, data={"source": "cli"})  # missing config_json (first required)
C("_stored_policy", {}, data=FULL_REQ)
C("_stored_policy", {"logger.warning": None}, data=dict(FULL_REQ, obsolete="x"))
C("_stored_policy", {}, data=dict(FULL_REQ, safe_mode=True, edit_approval_policy="session"))
C("_stored_policy", {"logger.warning": None}, data={"source": "cli", "bogus": 1})
C("_stored_policy", {}, data={"source": "cli", "bogus": 1})  # fail-closed warning
C("_stored_policy", {"logger.warning": R("ValueError", "x")}, data=FULL_REQ)

# -- _check_stored_surface --
C("_check_stored_surface", {"is_forward_policy": False}, policy={"source": "cli", "platform": "cli"})
C("_check_stored_surface", {"is_forward_policy": False}, policy={"source": "gui", "platform": "desktop"})
C("_check_stored_surface", {"is_forward_policy": False}, policy={"source": "local", "platform": "cli"})
C("_check_stored_surface", {"is_forward_policy": False}, policy={"source": "local", "platform": "unknown"})
C("_check_stored_surface", {"is_forward_policy": False}, policy={"source": "cli", "platform": "wrong"})
C("_check_stored_surface", {"is_forward_policy": False}, policy={"source": "cron", "platform": "cron"})
C("_check_stored_surface", {"is_forward_policy": False}, policy={"source": "bot_room", "platform": "bot_room"})
C("_check_stored_surface", {"is_forward_policy": True}, policy={"source": "x", "platform": "y"})
C("_check_stored_surface", {}, policy={"source": 5, "platform": "cli"})  # non-str -> no seam
C("_check_stored_surface", {}, policy={"source": "x", "platform": "y"})  # fail-closed
C("_check_stored_surface", {"is_forward_policy": R("SystemExit", "bye")}, policy={"source": "cli", "platform": "cli"})

# -- _extract_config_secrets --
C("_extract_config_secrets", {}, value={"api_key": "k"}, private={})
C("_extract_config_secrets", {"_is_secret_payload_key": False}, value={"model": {"api_key": "k"}}, private={})
C("_extract_config_secrets", {"_is_secret_payload_key": False}, value={"name": "x"}, private={})
C("_extract_config_secrets", {"_is_secret_payload_key": True}, value={"name": "x"}, private={})
C("_extract_config_secrets", {}, value={"api_key": "k"}, private=None)
C("_extract_config_secrets", {}, value={"env": {"FOO": "bar"}}, private={})
C("_extract_config_secrets", {}, value={"api_key": "{}"}, private={})
C("_extract_config_secrets", {}, value={"api_key": ""}, private={})
C("_extract_config_secrets", {}, value=[{"token": "t"}], private={})
C("_extract_config_secrets", {"_is_secret_payload_key": R("ValueError", "x")}, value={"name": "x"}, private={})
C("_extract_config_secrets", {}, value={"name": "x"}, private={})  # fail-closed
C("_extract_config_secrets", {}, value={"password": "p", "key": "k"}, private={})
C("_extract_config_secrets", {}, value={"api_key": "k"}, private={pk("pre"): "v"})

# -- render_launch_skills --
C("render_launch_skills", {"launch_skills": [[]]}, params={}, session_id="s")
C("render_launch_skills", {"launch_skills": [["a"]], "build_preloaded_skills_prompt": [["prompt", ["a"], []]]},
  params={"skills": ["a"]}, session_id="s")
C("render_launch_skills", {"launch_skills": [["a"]], "build_preloaded_skills_prompt": [["prompt", [], []]]},
  params={"skills": ["a"]}, session_id="s")
C("render_launch_skills", {"launch_skills": [["a"]], "build_preloaded_skills_prompt": [["prompt", ["a"], ["b"]]],
                           "format_missing_skills": "MISSING", "logger.warning": None},
  params={"skills": ["a"]}, session_id="s")
C("render_launch_skills", {"launch_skills": [["a"]], "build_preloaded_skills_prompt": [["", ["a"], []]]},
  params={"skills": ["a"]}, session_id="s")
C("render_launch_skills", {}, params={}, session_id="s")  # fail-closed launch_skills
C("render_launch_skills", {"launch_skills": AR("invalid_params", "skills", "bad")}, params={}, session_id="s")
C("render_launch_skills", {"launch_skills": [["a"]], "build_preloaded_skills_prompt": R("SystemExit", "bye")},
  params={"skills": ["a"]}, session_id="s")
C("render_launch_skills", {"launch_skills": [["a"]], "build_preloaded_skills_prompt": [["p", ["a"], ["b"]]],
                           "format_missing_skills": R("ValueError", "x")},
  params={"skills": ["a"]}, session_id="s")
C("render_launch_skills", {"launch_skills": [["a"]], "build_preloaded_skills_prompt": [[1, 2]]},
  params={"skills": ["a"]}, session_id="s")

# -- bind_launch_key --
C("bind_launch_key", {"bind_editor_mcp": BASE_POLICY},
  authority={"instance_id": "i", "epoch": "e"}, session_id="s", policy=BASE_POLICY,
  api_key=None, config_secrets={})
C("bind_launch_key", {"bind_editor_mcp": BASE_POLICY, "fingerprint_secret_value": "fp"},
  authority={"instance_id": "i", "epoch": "e"}, session_id="s", policy=BASE_POLICY,
  api_key="k", config_secrets={})
C("bind_launch_key", {"bind_editor_mcp": BASE_POLICY, "hmac.compare_digest": False},
  authority={"instance_id": "i", "epoch": "e", "_local_launch_keys": {"i:e:s": "old"}},
  session_id="s", policy=BASE_POLICY, api_key="k", config_secrets={})
C("bind_launch_key", {"bind_editor_mcp": BASE_POLICY, "hmac.compare_digest": True,
                      "fingerprint_secret_value": "fp"},
  authority={"instance_id": "i", "epoch": "e", "_local_launch_keys": {"i:e:s": "k"}},
  session_id="s", policy=BASE_POLICY, api_key="k", config_secrets={})
C("bind_launch_key", {"bind_editor_mcp": BASE_POLICY, "same_create_retry": True,
                      "hmac.compare_digest": True, "fingerprint_secret_value": "fp"},
  authority={"instance_id": "i", "epoch": "e", "_local_launch_keys": {"i:e:s": "old"},
             "_local_owner_key_requests": {"i:e:s": {"request_json": "{}"}}},
  session_id="s", policy=BASE_POLICY, api_key="new", owner_key=True, config_secrets={})
C("bind_launch_key", {"bind_editor_mcp": BASE_POLICY, "config_reference": "cref",
                      "fingerprint_secret_value": "fp"},
  authority={"instance_id": "i", "epoch": "e", "persistence": {}}, session_id="s",
  policy=BASE_POLICY, api_key="k", config_secrets={pk("model", "api_key"): "ck"})
C("bind_launch_key", {"bind_editor_mcp": BASE_POLICY},
  authority={"instance_id": "i", "epoch": "e",
             "_local_config_secrets": {"i:e:s": {pk("x"): "different"}}},
  session_id="s", policy=BASE_POLICY, api_key="k", config_secrets={pk("model", "api_key"): "ck"})
C("bind_launch_key", {}, authority={"instance_id": "i", "epoch": "e"}, session_id="s",
  policy=BASE_POLICY, api_key="k", config_secrets={})  # fail-closed bind_editor_mcp
C("bind_launch_key", {"bind_editor_mcp": R("SystemExit", "bye")},
  authority={"instance_id": "i", "epoch": "e"}, session_id="s", policy=BASE_POLICY,
  api_key="k", config_secrets={})
C("bind_launch_key", {"bind_editor_mcp": BASE_POLICY, "fingerprint_secret_value": R("ValueError", "x")},
  authority={"instance_id": "i", "epoch": "e"}, session_id="s", policy=BASE_POLICY,
  api_key="k", config_secrets={})
C("bind_launch_key", {"bind_editor_mcp": BASE_POLICY, "fingerprint_secret_value": "fp"},
  authority={"instance_id": "i", "epoch": "e"}, session_id="s", policy=BASE_POLICY,
  api_key="k", config_secrets={pk("editor_mcp"): {"srv": 1}})

# -- rebind_launch_key --
C("rebind_launch_key", {}, authority={}, session={"session_id": "s"}, api_key=None)
C("rebind_launch_key", {}, authority={}, session={"session_id": "s"}, api_key="   ")
C("rebind_launch_key", {"authority_persistence": {"receipt_policy": {}},
                        "restore_policy": {"credential_ref": "i:e:s#fp"}, "_key_slot": "i:e:s",
                        "fingerprint_secret_value": "fp", "hmac.compare_digest": True},
  authority={"sessions": {"s": {"source": {"platform": "local"}}}, "instance_id": "i", "epoch": "e"},
  session={"session_id": "s"}, api_key="k")
C("rebind_launch_key", {"authority_persistence": {"receipt_policy": {}},
                        "restore_policy": {"credential_ref": "i:e:s#fp"}, "_key_slot": "i:e:s",
                        "hmac.compare_digest": True},
  authority={"sessions": {"s": {"source": {"platform": "local"}}},
             "_local_launch_keys": {"i:e:s": "k"}},
  session={"session_id": "s"}, api_key="k")
C("rebind_launch_key", {"authority_persistence": {"receipt_policy": {}},
                        "restore_policy": {"credential_ref": "i:e:s#fp"}, "_key_slot": "i:e:s",
                        "hmac.compare_digest": False},
  authority={"sessions": {"s": {"source": {"platform": "local"}}},
             "_local_launch_keys": {"i:e:s": "other"}},
  session={"session_id": "s"}, api_key="k")
C("rebind_launch_key", {"authority_persistence": {"receipt_policy": {}},
                        "restore_policy": {"credential_ref": None}},
  authority={"sessions": {"s": {"source": {"platform": "local"}}}},
  session={"session_id": "s"}, api_key="k")
C("rebind_launch_key", {}, authority={"sessions": {"s": {"source": {"platform": "cli"}}}},
  session={"session_id": "s"}, api_key="k")
C("rebind_launch_key", {"authority_persistence": {"receipt_policy": {}},
                        "restore_policy": {"credential_ref": "i:e:s"}, "_key_slot": "i:e:s"},
  authority={"sessions": {"s": {"source": {"platform": "local"}}}},
  session={"session_id": "s"}, api_key="k")
C("rebind_launch_key", {}, authority={"sessions": {"s": {"source": {"platform": "local"}}}},
  session={"session_id": "s"}, api_key="k")  # fail-closed authority_persistence
C("rebind_launch_key", {"authority_persistence": {"receipt_policy": {}},
                        "restore_policy": R("SystemExit", "bye")},
  authority={"sessions": {"s": {"source": {"platform": "local"}}}},
  session={"session_id": "s"}, api_key="k")

# -- _secret_refs --
C("_secret_refs", {}, authority={"sessions": {}}, retired=[])
C("_secret_refs", {"local_adapter_map": {"local": {"policies": {"c1": {"credential_ref": "i:e:s#fp", "config_secret_ref": "cref"}}}},
                   "_key_slot": "i:e:s"},
  authority={"sessions": {"s": {"source": {"platform": "local", "chat_id": "c1"}}}}, retired=[])
C("_secret_refs", {}, authority={"sessions": {"s": {"source": {"platform": "local", "chat_id": "c1"}}}}, retired=["s"])
C("_secret_refs", {"local_adapter_map": {"local": {"policies": {"r1": {"config_secret_ref": "cref"}}}}},
  authority={"sessions": {}, "retired_local": ["r1"]}, retired=[])
C("_secret_refs", {"local_adapter_map": {"local": {"policies": {"c1": {"credential_ref": None, "config_secret_ref": "cref"}}}}},
  authority={"sessions": {"s": {"source": {"platform": "local", "chat_id": "c1"}}}}, retired=[])
C("_secret_refs", {}, authority={"sessions": {"s": {"source": {"platform": "local", "chat_id": "c1"}}}}, retired=[])  # fail-closed
C("_secret_refs", {"local_adapter_map": R("SystemExit", "bye")},
  authority={"sessions": {"s": {"source": {"platform": "local", "chat_id": "c1"}}}}, retired=[])
C("_secret_refs", {"local_adapter_map": {"local": {"policies": {"c1": {"credential_ref": "r1#fp"}, "c2": {"credential_ref": None, "config_secret_ref": "cref2"}}}},
                   "_key_slot": "i:e:s1"},
  authority={"sessions": {"s1": {"source": {"platform": "local", "chat_id": "c1"}},
                          "s2": {"source": {"platform": "local", "chat_id": "c2"}}}}, retired=[])

# -- release_launch_secrets --
C("release_launch_secrets", {}, authority={"instance_id": "i", "epoch": "e", "profile_id": "p"}, session_ids=[])
C("release_launch_secrets", {"hashlib.sha256": "HEX"},
  authority={"instance_id": "i", "epoch": "e", "profile_id": "p", "sessions": {},
             "_local_launch_keys": {"i:e:s": "k"}, "_local_editor_mcp": {"editor-session:HEX": {}}},
  session_ids=["s"])
C("release_launch_secrets", {"local_adapter_map": {"local": {"policies": {"c1": {"credential_ref": "i:e:s#fp"}}}},
                             "_key_slot": "i:e:s", "hashlib.sha256": "HEX"},
  authority={"instance_id": "i", "epoch": "e", "profile_id": "p",
             "sessions": {"other": {"source": {"platform": "local", "chat_id": "c1"}}},
             "_local_launch_keys": {"i:e:s": "k"}},
  session_ids=["s"])
C("release_launch_secrets", {"hashlib.sha256": "HEX"},
  authority={"instance_id": "i", "epoch": "e", "profile_id": "p", "sessions": {},
             "_local_config_secrets": {"profile-config-v1:{\"session\":\"s\"}": {}}},
  session_ids=["s"])
C("release_launch_secrets", {"hashlib.sha256": "HEX"},
  authority={"instance_id": "i", "epoch": "e", "profile_id": "p", "sessions": {},
             "_local_config_secrets": {"i:e:s": {}}},
  session_ids=["s"])
C("release_launch_secrets", {},
  authority={"instance_id": "i", "epoch": "e", "profile_id": "p",
             "sessions": {"other": {"source": {"platform": "local", "chat_id": "c1"}}}},
  session_ids=["s"])  # fail-closed local_adapter_map (via _secret_refs)
C("release_launch_secrets", {"hashlib.sha256": R("SystemExit", "bye")},
  authority={"instance_id": "i", "epoch": "e", "profile_id": "p", "sessions": {}},
  session_ids=["s"])

# -- policy_for_source --
C("policy_for_source", {}, runner={}, source=None)
C("policy_for_source", {}, runner={}, source={"platform": "cli"})
C("policy_for_source", {"runner._adapter_for_source": {"policies": {"c1": {"source": "cli"}}},
                        "adapter.authorize_source": True},
  runner={}, source={"platform": "local", "chat_id": "c1"})
C("policy_for_source", {"runner._adapter_for_source": "foreign"},
  runner={}, source={"platform": "local", "chat_id": "c1"})
C("policy_for_source", {"runner._adapter_for_source": {"policies": {}}, "adapter.authorize_source": False},
  runner={}, source={"platform": "local", "chat_id": "c1"})
C("policy_for_source", {"runner._adapter_for_source": {"policies": {}}, "adapter.authorize_source": True},
  runner={}, source={"platform": "local", "chat_id": "c1"})
C("policy_for_source", {}, runner={}, source={"platform": "local", "chat_id": "c1"})  # fail-closed
C("policy_for_source", {"runner._adapter_for_source": R("SystemExit", "bye")},
  runner={}, source={"platform": "local", "chat_id": "c1"})

# -- workspace_unavailable_result --
C("workspace_unavailable_result", {"t": "TEXT", "stamp_failure": {"stamped": True}}, policy={"cwd": "/x"})
C("workspace_unavailable_result", {"t": R("ValueError", "x")}, policy={"cwd": "/x"})
C("workspace_unavailable_result", {"t": "TEXT", "stamp_failure": R("SystemExit", "bye")}, policy={"cwd": "/x"})
C("workspace_unavailable_result", {}, policy={"cwd": "/x"})  # fail-closed t
C("workspace_unavailable_result", {"t": "T", "stamp_failure": {}}, policy={"cwd": None})
C("workspace_unavailable_result", {"t": "", "stamp_failure": {"final_response": ""}}, policy={"cwd": "/x"})

# -- refused_turn_result --
C("refused_turn_result", {"t": "T", "stamp_failure": {"r": 1}, "build_error_surface_from_result": {"surf": 1}},
  policy={"cwd": "/x"}, exc={"reason": "workspace_unavailable"})
C("refused_turn_result", {"launch_credentials_copy": "TEXT", "stamp_failure": {"r": 1},
                          "build_error_surface_from_result": {"surf": 1}},
  policy={"cwd": "/x"}, exc={"reason": "launch_credentials_unavailable"})
C("refused_turn_result", {}, policy={"cwd": "/x"}, exc={"reason": "other"})
C("refused_turn_result", {"t": R("ValueError", "x")}, policy={"cwd": "/x"}, exc={"reason": "workspace_unavailable"})
C("refused_turn_result", {"launch_credentials_copy": R("SystemExit", "bye")},
  policy={"cwd": "/x"}, exc={"reason": "launch_credentials_unavailable"})
C("refused_turn_result", {}, policy={"cwd": "/x"}, exc={"reason": "workspace_unavailable"})  # fail-closed t
C("refused_turn_result", {"launch_credentials_copy": "T", "stamp_failure": {"r": 1},
                          "build_error_surface_from_result": R("ValueError", "x")},
  policy={"cwd": "/x"}, exc={"reason": "launch_credentials_unavailable"})

# -- release_session_workspaces --
C("release_session_workspaces", {"owner_scope.__enter__": None, "owner_scope.__exit__": None},
  authority={}, session_ids=[])
C("release_session_workspaces", {"owner_scope.__enter__": None, "unbind_turn_workspace": [None, None],
                                 "owner_scope.__exit__": None},
  authority={}, session_ids=["t1", "t2"])
C("release_session_workspaces", {"owner_scope.__enter__": R("ValueError", "x")}, authority={}, session_ids=["t1"])
C("release_session_workspaces", {"owner_scope.__enter__": None, "unbind_turn_workspace": R("ValueError", "boom"),
                                 "owner_scope.__exit__": False},
  authority={}, session_ids=["t1"])
C("release_session_workspaces", {"owner_scope.__enter__": None, "unbind_turn_workspace": R("ValueError", "boom"),
                                 "owner_scope.__exit__": True},
  authority={}, session_ids=["t1"])
C("release_session_workspaces", {"owner_scope.__enter__": None, "unbind_turn_workspace": R("ValueError", "boom"),
                                 "owner_scope.__exit__": R("SystemExit", "bye")},
  authority={}, session_ids=["t1"])
C("release_session_workspaces", {"owner_scope.__enter__": None, "owner_scope.__exit__": R("ValueError", "x")},
  authority={}, session_ids=[])
C("release_session_workspaces", {}, authority={}, session_ids=[])  # fail-closed enter

# -- policy_turn_toolsets --
C("policy_turn_toolsets", {"policy.current_toolsets": [["a", "b"]], "onboarding_chat_toolsets": [["a", "b", "onboarding"]]},
  authority=None, policy={}, config={}, session_id="s")
C("policy_turn_toolsets", {"authority_persistence": {"store": 1}, "policy.current_toolsets": [["a"]],
                           "onboarding_chat_toolsets": [["a", "onboarding"]]},
  authority={"id": "a"}, policy={}, config={}, session_id="s")
C("policy_turn_toolsets", {"policy.current_toolsets": None}, authority=None, policy={}, config={}, session_id="s")
C("policy_turn_toolsets", {}, authority=None, policy={}, config={}, session_id="s")  # fail-closed current_toolsets
C("policy_turn_toolsets", {}, authority={"id": "a"}, policy={}, config={}, session_id="s")  # fail-closed authority_persistence
C("policy_turn_toolsets", {"policy.current_toolsets": [["a"]], "onboarding_chat_toolsets": R("SystemExit", "bye")},
  authority=None, policy={}, config={}, session_id="s")
C("policy_turn_toolsets", {"policy.current_toolsets": R("ValueError", "x")},
  authority=None, policy={}, config={}, session_id="s")


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=True) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    short = {k: v for k, v in counts.items() if v < 5}
    print("wrote %d cases to %s" % (len(CASES), OUT))
    print("funcs: %d, min cases/func: %d" % (len(counts), min(counts.values())))
    if short:
        raise SystemExit("FUNCS WITH <5 CASES: %s" % short)


if __name__ == "__main__":
    main()
