#!/usr/bin/env python3
import os,re
ROOT=os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MGR=os.path.join(ROOT,"src","desktop_app_common.c")
SET=os.path.join(ROOT,"src","desktop_settings.c")
STH=os.path.join(ROOT,"src","desktop_state.h")

m=open(MGR).read()
# 1. includes
m=m.replace('#include "hermes.h"\n','#include "desktop_state.h"\n#include "hermes_logger.h"\n',1)
# 2. replace static struct {...} g_desktop = {0}; block with desktop_state_t
m=re.sub(r'static struct \{.*?\} g_desktop = \{0\};',
         '/* Shared desktop state instance — type defined in desktop_state.h. */\ndesktop_state_t g_desktop = {0};',
         m, count=1, flags=re.S)
# 3. drop static from shared helpers
for fn in ['void notify_status','int find_session_by_id','int find_model_by_id','int find_profile_by_name']:
    m=m.replace('static '+fn, fn, 1)
open(MGR,"w").write(m)
print("manager patched")

# 4. state.h: add extern decls for shared path helpers + g_safe moved type
sth=open(STH).read()
extra='''extern const char *desktop_settings_path(void);
extern const char *desktop_sessions_path(void);
extern const char *desktop_profiles_dir(void);
extern const char *desktop_safe_storage_path(void);
extern const char *desktop_config_dir(void);
extern void dir_create(const char *path);
'''
if 'desktop_settings_path' not in sth:
    sth=sth.replace('void notify_status(const char *fmt, ...);\n',
                    'void notify_status(const char *fmt, ...);\n'+extra)
    open(STH,"w").write(sth)
    print("state.h: added shared-helper externs")

# 5. move safe-storage state into settings.c (g_safe/safe_entry_t/MAX_SAFE_ENTRIES)
s=open(SET).read()
# extract the safe storage state block from manager
mm=open(MGR).read()
blk=re.search(r'typedef struct \{\n.*?\} safe_entry_t;.*?\n\} g_safe = \{0\};', mm, re.S)
if blk:
    state_block=blk.group(0)
    # remove from manager
    mm=mm.replace(blk.group(0),'/* safe-storage state moved to desktop_settings.c */')
    open(MGR,"w").write(mm)
    # prepend to settings.c (after includes)
    s=re.sub(r'(#include "desktop_state.h"\n)',
             r'\1\n'+state_block+'\n', s, count=1)
    open(SET,"w").write(s)
    print("moved safe-storage state into desktop_settings.c")
else:
    print("WARN: safe_entry_t block not found in manager")
