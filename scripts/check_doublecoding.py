import re
extracted_modules = {
 'skills_sync_fs':'src/tools/skills_sync_fs.c',
 'file_text_ops':'src/tools/file_text_ops.c',
 'file_pagination_ops':'src/tools/file_pagination_ops.c',
 'file_fs_ops':'src/tools/file_fs_ops.c',
 'file_ops_lint':'src/tools/file_ops_lint.c',
 'cron_prompt_sanitize':'src/tools/cron_prompt_sanitize.c',
 'send_message_target':'src/tools/send_message_target.c',
 'web_base64_img':'src/tools/web_base64_img.c',
 'image_gen_path':'src/tools/image_gen_path.c',
 'image_gen_provider':'src/tools/image_gen_provider.c',
}
mod_syms=set()
for m,f in extracted_modules.items():
    try:
        src=open(f).read()
    except Exception:
        continue
    for l in src.splitlines():
        mm=re.match(r'^(?:static\s+)?(?:const\s+)?[a-zA-Z_][\w\s\*]*?\b([a-z_][\w]*)\s*\(',l)
        if mm: mod_syms.add(mm.group(1))

targets=['port_image_generation_tool','port_cronjob_tools','port_send_message_tool','port_browser_supervisor','port_web_tools','port_file_operations']
for t in targets:
    src=open('src/tools/%s.c' % t).read()
    defs=set(re.findall(r'^(?:static\s+)?(?:const\s+)?[a-zA-Z_][\w\s\*]*?\b([a-z_][\w]*)\s*\(',src,re.M))
    overlap=sorted(defs & mod_syms)
    if overlap:
        print('%s: overlap-with-modules=%s' % (t, overlap))
        for fn in overlap:
            # does the file DELEGATE (call fn with same name inside?) - crude: check if it calls a module-prefixed variant
            print('    %s present in both' % fn)
    else:
        print('%s: no name overlap with extracted modules (no obvious double-coding)' % t)
