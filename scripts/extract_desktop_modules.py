#!/usr/bin/env python3
"""Regenerate the 4 concern modules from a pristine HEAD desktop_app_common.c.
Extracts each function body verbatim (correct brace-end), writes desktop_<x>.c
with minimal includes + desktop_state.h, and writes desktop_state.h."""
import os,re
ROOT=os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC=os.path.join(ROOT,"src","desktop_app_common.c")
BASE=os.path.join(ROOT,"src")
lines=open(SRC).read().splitlines()
def brace_end(start):
    depth=0;started=False;i=start;n=len(lines);in_blk=False
    while i<n:
        line=lines[i];j=0;L=len(line)
        while j<L:
            c=line[j];nx=line[j+1] if j+1<L else ''
            if in_blk:
                if c=='*' and nx=='/':in_blk=False;j+=2;continue
                j+=1;continue
            if c=='/' and nx=='/':break
            if c=='/' and nx=='*':in_blk=True;j+=2;continue
            if c=='"' or c=="'":
                q=c;j+=1
                while j<L:
                    if line[j]=='\\':j+=2;continue
                    if line[j]==q:break
                    j+=1
                j+=1;continue
            if c=='{':depth+=1;started=True
            elif c=='}':
                depth-=1
                if started and depth==0:return i+1
            j+=1
        i+=1
    return i
def get_body(s):
    e=brace_end(s-1)
    return "\n".join(lines[s-1:e])+"\n"
MAP={
 'sessions':['desktop_session_create','desktop_session_delete','desktop_session_select','desktop_session_rename','desktop_session_archive','desktop_session_unarchive','desktop_session_pin','desktop_session_list','desktop_session_count','desktop_session_search','desktop_session_export','desktop_session_enable_dnd','desktop_session_has_dnd'],
 'models':['desktop_model_list','desktop_model_select','desktop_model_refresh','desktop_model_active_id','desktop_model_active','desktop_model_find','desktop_model_analytics_get','desktop_model_analytics_list','desktop_model_analytics_reset','desktop_model_set_visibility','desktop_model_get_visibility','desktop_auxiliary_model_set','desktop_auxiliary_model_list','desktop_auxiliary_model_for_task'],
 'profiles':['desktop_profile_list','desktop_profile_create','desktop_profile_delete','desktop_profile_rename','desktop_profile_select','desktop_profile_set_soul','desktop_profile_get_soul','desktop_profile_set_model','desktop_profile_active','desktop_profile_find','desktop_profile_set_scope','desktop_profile_get_scope'],
 'settings':['find_setting','desktop_settings_get','desktop_settings_set','desktop_settings_get_int','desktop_settings_set_int','desktop_settings_get_bool','desktop_settings_set_bool','desktop_settings_list','desktop_settings_load','desktop_settings_save','desktop_settings_get_theme','desktop_settings_set_theme','desktop_settings_get_gateway_url','desktop_settings_set_gateway_url','safe_storage_load','safe_storage_save','desktop_safe_storage_set','desktop_safe_storage_get','desktop_safe_storage_delete'],
}
LINES={}
for nm in sum(MAP.values(),[]):
    # find signature line
    for i,l in enumerate(lines):
        if re.match(r'^(?:static\s+)?[A-Za-z_][\w\s\*]*?\b'+re.escape(nm)+r'\s*\(', l):
            LINES[nm]=i+1; break
INC={
 'sessions':['#include <stdio.h>','#include <stdlib.h>','#include <string.h>','#include <stdbool.h>','#include <time.h>','#include "hermes_json.h"','#include "hermes_logger.h"','#include "desktop_state.h"'],
 'models':['#include <stdio.h>','#include <stdlib.h>','#include <string.h>','#include <stdbool.h>','#include "hermes_json.h"','#include "desktop_state.h"'],
 'profiles':['#include <stdio.h>','#include <stdlib.h>','#include <string.h>','#include <stdbool.h>','#include "hermes_json.h"','#include "desktop_state.h"'],
 'settings':['#include <stdio.h>','#include <stdlib.h>','#include <string.h>','#include <stdbool.h>','#include <unistd.h>','#include <sys/stat.h>','#include "hermes_json.h"','#include "desktop_state.h"'],
}
for mod,names in MAP.items():
    inc="\n".join(INC[mod])
    c=f"/*\n * desktop_{mod}.c — concern module extracted from desktop_app_common.c.\n * Self-contained, operates on shared g_desktop (desktop_state.h), C11.\n */\n\n{inc}\n\n"
    for nm in names:
        if nm not in LINES: print("WARN missing",nm);continue
        body=get_body(LINES[nm])
        body=re.sub(r'^(static\s+)+','',body,flags=re.M)
        body=re.sub(r'(?m)^\s*static\s+',' ',body)
        c+=body+"\n"
    open(os.path.join(BASE,f"desktop_{mod}.c"),"w").write(c)
    print(f"wrote desktop_{mod}.c ({len(names)} fns)")
state_h='''#ifndef DESKTOP_STATE_H
#define DESKTOP_STATE_H
#include "desktop_app.h"
typedef struct {
    desktop_session_t sessions[DESKTOP_MAX_SESSIONS];
    int session_count; int active_session;
    desktop_model_t models[DESKTOP_MAX_MODELS];
    int model_count; int active_model;
    desktop_profile_t profiles[DESKTOP_MAX_PROFILES];
    int profile_count; int active_profile;
    desktop_setting_t settings[DESKTOP_MAX_SETTINGS];
    int setting_count; desktop_theme_t theme;
    desktop_notification_t notifications[DESKTOP_MAX_NOTIFICATIONS];
    int notification_count;
    char gateway_url[1024]; char gateway_token[2048]; bool connected;
    char auth_ticket[2048]; bool auth_valid;
    desktop_update_info_t update_info;
    bool running; void (*status_cb)(const char *status);
#ifdef _WIN32
    HANDLE lock_handle;
#else
    int lock_fd;
#endif
    bool session_dnd_enabled;
} desktop_state_t;
extern desktop_state_t g_desktop;
int find_session_by_id(const char *id);
int find_model_by_id(const char *id);
int find_profile_by_name(const char *name);
struct desktop_setting_t *find_setting(const char *key);
void notify_status(const char *fmt, ...);
#endif
'''
open(os.path.join(BASE,"desktop_state.h"),"w").write(state_h)
print("wrote desktop_state.h")
