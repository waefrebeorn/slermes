#!/usr/bin/env python3
"""Delete 4 concern blocks from desktop_app_common.c by AUTHORITATIVE original
line numbers (start = function signature line; end = matching closing brace,
found by a correct per-char brace counter). Then assert balance. Modules hold
the verbatim bodies already."""
import os,re
ROOT=os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC=os.path.join(ROOT,"src","desktop_app_common.c")
lines=open(SRC).read().splitlines()

def brace_end(start):
    depth=0;started=False
    i=start; n=len(lines)
    in_blk=False  # multi-line block comment state
    while i<n:
        line=lines[i]; j=0; L=len(line)
        while j<L:
            c=line[j]; nx=line[j+1] if j+1<L else ''
            if in_blk:
                if c=='*' and nx=='/':
                    in_blk=False; j+=2; continue
                j+=1; continue
            if c=='/' and nx=='/':
                break
            if c=='/' and nx=='*':
                in_blk=True; j+=2; continue
            if c=='"' or c=="'":
                q=c; j+=1
                while j<L:
                    if line[j]=='\\': j+=2; continue
                    if line[j]==q: break
                    j+=1
                j+=1; continue
            if c=='{': depth+=1; started=True
            elif c=='}':
                depth-=1
                if started and depth==0: return i+1
            j+=1
        i+=1
    return i

# authoritative (start, name, end_override?) -> end via brace_end, with manual
# overrides for functions containing '}' inside char literals (scanner edge).
GROUPS=[
 (218,'desktop_session_create'),(253,'desktop_session_delete'),(282,'desktop_session_select'),
 (291,'desktop_session_rename'),(304,'desktop_session_archive'),(313,'desktop_session_unarchive'),
 (322,'desktop_session_pin'),(329,'desktop_session_list'),(348,'desktop_session_count'),
 (353,'desktop_session_search'),(1292,'desktop_session_export'),(1379,'desktop_session_enable_dnd'),
 (1383,'desktop_session_has_dnd'),
 (372,'desktop_model_list'),(380,'desktop_model_select'),(401,'desktop_model_refresh'),
 (436,'desktop_model_active_id'),(442,'desktop_model_active'),(448,'desktop_model_find'),
 (1589,'desktop_model_analytics_get'),(1600,'desktop_model_analytics_list'),(1607,'desktop_model_analytics_reset'),
 (1623,'desktop_model_set_visibility'),(1629,'desktop_model_get_visibility'),
 (1549,'desktop_auxiliary_model_set'),(1569,'desktop_auxiliary_model_list'),(1576,'desktop_auxiliary_model_for_task'),
 (458,'desktop_profile_list'),(466,'desktop_profile_create'),(521,'desktop_profile_delete'),
 (554,'desktop_profile_rename'),(577,'desktop_profile_select'),(586,'desktop_profile_set_soul'),
 (602,'desktop_profile_get_soul'),(623,'desktop_profile_set_model'),(638,'desktop_profile_active'),
 (644,'desktop_profile_find'),(1534,'desktop_profile_set_scope'),(1540,'desktop_profile_get_scope'),
 (655,'find_setting'),(663,'desktop_settings_get'),(671,'desktop_settings_set'),
 (685,'desktop_settings_get_int'),(692,'desktop_settings_set_int'),(703,'desktop_settings_get_bool'),
 (710,'desktop_settings_set_bool'),(721,'desktop_settings_list'),
 (728,'desktop_settings_load',781),(783,'desktop_settings_save',812),
 (815,'desktop_settings_get_theme'),(819,'desktop_settings_set_theme'),
 (827,'desktop_settings_get_gateway_url'),(833,'desktop_settings_set_gateway_url'),
 (968,'safe_storage_load',1004),(1006,'safe_storage_save',1016),
 (1018,'desktop_safe_storage_set'),(1035,'desktop_safe_storage_get'),(1046,'desktop_safe_storage_delete'),
]
ranges=[]
for item in GROUPS:
    s=item[0]; name=item[1]
    e=item[2] if len(item)>2 else brace_end(s-1)
    ranges.append((s,e,name))
    print(f"  {name}: {s}-{e}")
# also delete the preceding PoP/comment line for each (one line up if it's a comment)
def prune_ranges():
    # delete ONLY the function [s,e] (signature line to closing brace).
    # Leave any preceding comment block in place (harmless; avoids deleting
    # braces that appear inside doc-comment code samples).
    return [(s,e) for s,e,name in ranges]
pr=prune_ranges()
srclines=list(lines)
for s,e in sorted(pr,reverse=True):
    del srclines[s-1:e]
out="\n".join(srclines)+"\n"
out=re.sub(r'\n{4,}','\n\n\n',out)
# correct brace-balance check (skip strings/comments/char-literals)
def bal(s):
    d=0;neg=0;in_blk=False;i=0;n=len(s)
    while i<n:
        c=s[i];nx=s[i+1] if i+1<n else ''
        if in_blk:
            if c=='*' and nx=='/':in_blk=False;i+=2;continue
            i+=1;continue
        if c=='/' and nx=='/':
            while i<n and s[i]!='\n':i+=1
            continue
        if c=='/' and nx=='*':in_blk=True;i+=2;continue
        if c=='"' or c=="'":
            q=c;i+=1
            while i<n:
                if s[i]=='\\':i+=2;continue
                if s[i]==q:break
                i+=1
            i+=1;continue
        if c=='{':d+=1
        elif c=='}':
            d-=1
            if d<0:neg+=1;d=0
        i+=1
    return d,neg
d,neg=bal(out)
if d!=0 or neg!=0:
    print(f"ABORT brace imbalance depth={d} neg={neg}")
    raise SystemExit(1)
open(SRC,"w").write(out)
print(f"manager pruned -> {len(srclines)} lines, brace-balanced OK")
