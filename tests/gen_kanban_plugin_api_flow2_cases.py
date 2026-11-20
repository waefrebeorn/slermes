#!/usr/bin/env python3
"""Generate tests/fixtures/kanban_plugin_api_flow2.jsonl (>=5 cases per func)."""
import json, os
from pathlib import Path

HERE=Path(__file__).resolve().parent
OUT=HERE/"fixtures"/"kanban_plugin_api_flow2.jsonl"

PARSED={"est_tokens":12,"complexity":" M ","rationale":"  because  "}
REC={"id":"t1","task_id":"t1","title":"Task One","status":"done","slug":"board1",
     "ended_at":None,"ok":True,"worker_pid":123,"current_run_id":9,"claim_lock":"lk",
     "worker_started_at":100,"gated":False,"chat_id":"chat1","thread_id":"thr1","name":"Home",
     "value":"telegram","primary_path":"/repo","project_id":"p1","default_workdir":"/work",
     "assignee":"alice","detail":"detail","parsed":PARSED,"model":"model-x",
     "est_tokens":12,"complexity":"M","rationale":"because","providers":[{"slug":"p","label":"P","models":["m"]}],
     "read":True,"profiles":[],"parsed_ok":True}
ROWS=[{"id":"t1","task_id":"t1","title":"Task One","status":"done","tenant":"acme"}]
CFG={"dashboard":{"kanban":{"default_tenant":"acme","lane_by_profile":True}},
     "kanban":{"orchestrator_profile":"  ","default_assignee":" bob ","auto_decompose":True}}

def base_io():
    io={
      "conn.execute":{"ret":ROWS}, "conn.close":{"ret":None},
      "_resolve_board":{"ret":"board1"}, "_conn":{"ret":dict(REC)},
      "importlib.import_module":{"ret":dict(REC)},
      "kanban_db.get_task":{"ret":dict(REC)}, "kanban_db.create_task":{"ret":"t1"},
      "kanban_db.delete_task":{"ret":True}, "kanban_db.delete_attachment":{"ret":dict(REC)},
      "kanban_db.list_attachments":{"ret":[dict(REC)]}, "kanban_db.list_comments":{"ret":[dict(REC)]},
      "kanban_db.list_events":{"ret":[dict(REC)]}, "kanban_db.list_runs":{"ret":[dict(REC)]},
      "kanban_db.list_tasks":{"ret":[dict(REC)]}, "kanban_db.list_boards":{"ret":[dict(REC)]},
      "kanban_db.list_known":{"ret":[dict(REC)]}, "kanban_db.known_assignees":{"ret":[{"name":"alice"}]},
      "kanban_db.latest_summary":{"ret":"summary"}, "kanban_db.latest_summaries":{"ret":dict(REC)},
      "kanban_db.current_run_started_ats":{"ret":dict(REC)}, "kanban_db.board_stats":{"ret":dict(REC)},
      "kanban_db.reclaim_task":{"ret":True}, "kanban_db.reassign_task":{"ret":True},
      "kanban_db.link_tasks":{"ret":True}, "kanban_db.unlink_tasks":{"ret":True},
      "kanban_db.add_attachment":{"ret":"a1"}, "kanban_db.get_attachment":{"ret":dict(REC)},
      "kanban_db.remove_board":{"ret":dict(REC)}, "kanban_db.get_current_board":{"ret":"board1"},
      "kanban_db.create_board":{"ret":dict(REC)}, "kanban_db.write_board_metadata":{"ret":dict(REC)},
      "kanban_db.set_current_board":{"ret":None}, "kanban_db.notify_task_updated":{"ret":None},
      "kanban_db.add_comment":{"ret":None}, "kanban_db.write_txn":{"ret":None},
      "kanban_db.kanban_db_path":{"ret":dict(REC)}, "kanban_db.task_attachments_dir":{"ret":dict(REC)},
      "kanban_db.read_worker_log":{"ret":"log content"}, "kanban_db._retry_status_for_run":{"ret":"review"},
      "kanban_db._parents_satisfied":{"ret":True}, "kanban_db._end_run":{"ret":9},
      "kanban_db.invalidate_descendants_for_parent_reopen":{"ret":{"terminations":[]}},
      "kanban_db._terminate_reclaimed_worker":{"ret":None}, "kanban_db.recompute_ready":{"ret":None},
      "kanban_db.scoped_current_board":{"ret":None}, "scoped_current_board.exit":{"ret":None},
      "kbd.dispatch_once":{"ret":dict(REC)},
      "kd.compute_task_diagnostics":{"ret":[{"severity":"warning","last_seen_at":5}]},
      "_require_task":{"ret":dict(REC)}, "_require_run":{"ret":dict(REC)},
      "_task_dict":{"ret":dict(REC)}, "_links_for":{"ret":{"children":["t2"],"parents":[]}},
      "_attach_diagnostics":{"ret":None}, "_link_tasks":{"ret":dict(REC)},
      "_apply_status":{"ret":True}, "_patch_status":{"ret":None}, "_patch_title_body":{"ret":None},
      "_set_priority":{"ret":None}, "_open_parent_refusal":{"ret":""},
      "_compute_task_diagnostics":{"ret":{"t1":[{"severity":"warning","last_seen_at":5}]}},
      "_configured_home_channels":{"ret":[dict(REC)]}, "_home_for_platform":{"ret":dict(REC)},
      "_existing_board_slug":{"ret":"board1"}, "_load_config_or_empty":{"ret":dict(CFG)},
      "_resolve_project":{"ret":["p1","Project","/repo"]}, "_projects_by_id":{"ret":{"p1":dict(REC)}},
      "_default_workspace_kind":{"ret":"dir"}, "_annotate_board_meta":{"ret":dict(REC)},
      "_validate_workdir":{"ret":"/work"}, "_run_aux":{"ret":dict(REC)}, "_run_estimate":{"ret":{"ok":True}},
      "_run_transfer":{"ret":dict(REC)}, "_read_board":{"ret":dict(REC)},
      "_ws_upgrade_authorized":{"ret":True}, "_ws_board":{"ret":"board1"},
      "_ws_board_live":{"ret":"board1"}, "_since_param":{"ret":5},
      "ws.accept":{"ret":None}, "ws.close":{"ret":None}, "tail.shutdown":{"ret":None},
      "tail._latest":{"ret":5}, "tail._fetch":{"ret":dict(REC)}, "tail._close":{"ret":None},
      "tail._run":{"ret":dict(REC)}, "tail.poll":{"ret":[5,[dict(REC)]]},
      "get_affinity_scope":{"ret":None}, "reset_affinity_scope":{"ret":None},
      "call_llm":{"ret":{"choices":[{"message":{"content":"{\"ok\": true}"}}],"model":"m1","parsed":dict(PARSED)}},
      "build_models_payload":{"ret":dict(REC)}, "load_config":{"ret":dict(CFG)}, "save_config":{"ret":None},
      "load_gateway_config":{"ret":{"platforms":{"telegram":{"value":"telegram","home_channel":dict(REC)}}}},
      "pdb.get_project":{"ret":dict(REC)}, "pdb.list_projects":{"ret":[dict(REC)]},
      "profiles_mod.list_profiles":{"ret":[dict(REC)]}, "profiles_mod.get_active_profile_name":{"ret":"alice"},
      "profiles_mod.normalize_profile_name":{"ret":"alice"}, "profiles_mod.profile_exists":{"ret":True},
      "profiles_mod.write_profile_meta":{"ret":None}, "profile_dir.is_dir":{"ret":True},
      "profile_describer.describe_profile":{"ret":dict(REC)},
      "kanban_workflow.DEFAULT_WORKFLOW.to_dict":{"ret":{"columns":[]}},
      "kbw._git_toplevel":{"ret":"/repo"}, "Path.is_dir":{"ret":True},
      "log_path.stat":{"ret":7}, "stored.relative_to":{"ret":None}, "stored.is_file":{"ret":True},
      "kbn.list_notify_subs":{"ret":[dict(REC)]}, "kbn.add_notify_sub":{"ret":None},
      "kbn.remove_notify_sub":{"ret":None},
      "run_in_executor":{"ret":dict(REC)},
      "_safe_attachment_name":{"ret":"file.pdf"}, "_collision_free_path":{"ret":dict(REC)},
      "dest.write":{"ret":None}, "body":{"ret":None},
    }
    return io

FUZZ={"__raise__":"RuntimeError","message":"boom"}
HTTP={"__raise__":"HTTPException","message":"409: conflict","status_code":409,"detail":"conflict"}

FUNCS=["_board_conn","_with_board_pinned","_run_aux","_map_errors","_errors_to_500",
 "_compute_task_diagnostics","get_board","get_board_endpoint","get_task","create_task",
 "list_task_attachments","upload_task_attachment","download_attachment","remove_attachment",
 "_patch_status","_patch_title_body","update_task","delete_task","_parents_blocking_ready",
 "_set_status_direct","add_comment","add_link","delete_link","_bulk_apply_one","bulk_update",
 "list_diagnostics","list_active_workers","get_run_endpoint","inspect_run_endpoint",
 "terminate_run_endpoint","reclaim_task_endpoint","specify_task_endpoint","reassign_task_endpoint",
 "estimate_text_endpoint","estimate_task_endpoint","_run_estimate","get_config",
 "_configured_home_channels","_home_for_platform","get_home_channels","subscribe_home",
 "unsubscribe_home","get_stats","get_assignees","get_task_log","dispatch","model_options",
 "_resolve_project","_projects_by_id","_board_counts","_default_workspace_kind","_annotate_board_meta",
 "list_kanban_projects","list_boards","_validate_workdir","create_board_endpoint","rename_board",
 "delete_board","_run_transfer","export_board_endpoint","import_board_endpoint","switch_board",
 "list_profile_roster","update_profile_description","auto_describe_profile","decompose_task_endpoint",
 "get_workflow","get_orchestration_settings","_validated_profile_name","set_orchestration_settings",
 "stream_events","_latest","_fetch","_close","_run","latest","poll","shutdown","model_options"]

COMMON={"board":"board1","task_id":"t1","run_id":9,"attachment_id":1,"slug":"board1",
        "parent_id":"t1","child_id":"t2","profile":"alice","platform":"telegram",
        "raw":"/work","ref":"p1","fn":"fn","module":"kanban_specify","author":"alice",
        "detail":"detail","payload":dict(REC),"goal":"g","cursor":5,"tail":5,
        "log_path":"/logs/t1.log","board_meta":dict(REC),"meta":dict(REC),
        "run_state_type":None,"run_state_name":None,"include_archived":False,
        "types":["ValueError","RuntimeError"],"status":400,"prefix":"oops","profiles_mod":dict(REC),
        "args":{"dry_run":False},"fn":"fn","ids":["t1"]}

def cases(func):
    out=[]
    # 1 base
    out.append({"func":func,"in":{**COMMON,"io":base_io()}})
    # 2 generic RuntimeError on body/primary seams
    io=base_io()
    for k in ("body","_resolve_board","_require_task","conn.execute"):
        io[k]={"raise":dict(FUZZ)}
    out.append({"func":func,"in":{**COMMON,"io":io}})
    # 3 HTTPException raise
    io=base_io()
    for k in ("_resolve_board","_require_task","conn.execute","body"):
        io[k]={"raise":dict(HTTP)}
    out.append({"func":func,"in":{**COMMON,"io":io,"run_state_type":"bogus","run_state_name":None}})
    # 4 empty/falsy seam results
    io=base_io()
    for k in ("conn.execute","_configured_home_channels","_load_config_or_empty"):
        io[k]={"ret":[] if k!="_load_config_or_empty" else {}}
    out.append({"func":func,"in":{**COMMON,"io":io,"task_ids":[],"ids":[]}})
    # 5 queue script (two calls)
    io=base_io()
    io["_resolve_board"]=[{"ret":"board1"},{"raise":dict(FUZZ)}]
    io["conn.execute"]=[{"ret":ROWS},{"ret":[]}]
    out.append({"func":func,"in":{**COMMON,"io":io,"board":None}})
    # 6 alternate scalar inputs + truthy overrides
    io=base_io()
    io["_resolve_board"]={"ret":None}
    io["_ws_upgrade_authorized"]={"ret":False}
    out.append({"func":func,"in":{**COMMON,"io":io,"board":None,"run_state_type":"status","run_state_name":"done",
                                  "task_ids":["t1"],"tail":None,"title":"Task","raw":"relative"}})
    return out

def main():
    OUT.parent.mkdir(parents=True,exist_ok=True)
    rows=[]
    for f in FUNCS:
        rows.extend(cases(f))
    with open(OUT,"w",encoding="utf-8",newline="\n") as fh:
        for r in rows: fh.write(json.dumps(r,ensure_ascii=False)+"\n")
    from collections import Counter
    cnt=Counter(r["func"] for r in rows)
    print(f"wrote {len(rows)} cases for {len(cnt)} funcs; min/case={min(cnt.values())}, max={max(cnt.values())}")

if __name__=="__main__": main()
