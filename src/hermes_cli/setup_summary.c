/*
 * setup_summary.c — faithful C11 port of the pure tool-status row builders in
 * upstream Hermes hermes_cli/setup_summary.py (gate w10_setupsumma_2652f3).
 *
 * PoP: w10_setupsumma_2652f3_managed_or_provider_row @ hermes_cli/setup_summary.py:_managed_or_provider_row
 * PoP: w10_setupsumma_2652f3_web_row                @ hermes_cli/setup_summary.py:_web_row
 * PoP: w10_setupsumma_2652f3_browser_row            @ hermes_cli/setup_summary.py:_browser_row
 * PoP: w10_setupsumma_2652f3_image_gen_row          @ hermes_cli/setup_summary.py:_image_gen_row
 * PoP: w10_setupsumma_2652f3_video_gen_row          @ hermes_cli/setup_summary.py:_video_gen_row
 * PoP: w10_setupsumma_2652f3_always_on_rows         @ hermes_cli/setup_summary.py:_always_on_rows
 *
 * Print/terminal effects and plugin discovery are out of scope; the plugin
 * provider probe is injected as an explicit `backend` seam.
 */
#include "noc_w10_setupsumma_2652f3.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WEB_MISSING "EXA_API_KEY, PARALLEL_API_KEY, FIRECRAWL_API_KEY/FIRECRAWL_API_URL, TAVILY_API_KEY, PERPLEXITY_API_KEY, KEENABLE_API_KEY, or SEARXNG_URL"
#define BROWSER_DEFAULT "Chromium (run 'hermes pm install chromium'), or configure Browser Use or Browserbase"

static int fbool(const json_t *f,const char *k){
    json_t *v = (f && f->type==JSON_OBJECT) ? json_obj_get(f,k) : NULL;
    return (v && v->type==JSON_BOOL) ? v->bool_val : 0;
}
static const char *fstr(const json_t *f,const char *k){
    json_t *v = (f && f->type==JSON_OBJECT) ? json_obj_get(f,k) : NULL;
    return (v && v->type==JSON_STRING && v->str_val && v->str_val[0]) ? v->str_val : "";
}
static json_t *row(const char *label,int avail,const char *hint){
    json_t *a=json_array();
    json_append(a,json_string(label));
    json_append(a,json_bool(avail));
    json_append(a,hint&&*hint?json_string(hint):json_null());
    return a;
}

json_t *w10_setupsumma_2652f3_managed_or_provider_row(const char *name,const char *managed_label,
                                                      const char *missing_hint,const json_t *feature){
    char b[512];
    if(fbool(feature,"managed_by_nous")){
        snprintf(b,sizeof(b),"%s (%s)",name,managed_label);
        return row(b,1,NULL);
    }
    if(fbool(feature,"available")){
        const char *cp=fstr(feature,"current_provider");
        if(cp&&*cp){snprintf(b,sizeof(b),"%s (%s)",name,cp);return row(b,1,NULL);}
        return row(name,1,NULL);
    }
    return row(name,0,missing_hint);
}
json_t *w10_setupsumma_2652f3_web_row(const json_t *feature){
    return w10_setupsumma_2652f3_managed_or_provider_row("Web Search & Extract","Nous subscription",WEB_MISSING,feature);
}
json_t *w10_setupsumma_2652f3_browser_row(const json_t *feature){
    const char *cp=fstr(feature,"current_provider");
    const char *hint=BROWSER_DEFAULT;
    if(cp&&*cp){
        if(!strcmp(cp,"Browserbase"))hint="BROWSERBASE_API_KEY/BROWSERBASE_PROJECT_ID";
        else if(!strcmp(cp,"Browser Use"))hint="BROWSER_USE_API_KEY";
        else if(!strcmp(cp,"Local browser"))hint="Chromium: run 'hermes pm install chromium'";
    }
    return w10_setupsumma_2652f3_managed_or_provider_row("Browser Automation","Nous Browser Use",hint,feature);
}
json_t *w10_setupsumma_2652f3_image_gen_row(const json_t *feature,const char *backend){
    char b[512];
    if(fbool(feature,"managed_by_nous"))return row("Image Generation (Nous subscription)",1,NULL);
    if(fbool(feature,"available"))return row("Image Generation",1,NULL);
    if(backend&&*backend){snprintf(b,sizeof(b),"Image Generation (%s)",backend);return row(b,1,NULL);}
    return row("Image Generation",0,"FAL_KEY or OPENAI_API_KEY");
}
json_t *w10_setupsumma_2652f3_video_gen_row(const json_t *feature,const char *backend){
    char b[512];
    if(fbool(feature,"managed_by_nous"))return row("Video Generation (FAL via Nous subscription)",1,NULL);
    if(backend&&*backend){snprintf(b,sizeof(b),"Video Generation (%s)",backend);return row(b,1,NULL);}
    return json_null();
}
json_t *w10_setupsumma_2652f3_always_on_rows(void){
    json_t *a=json_array();
    json_append(a,row("Terminal/Commands",1,NULL));
    json_append(a,row("Task Planning (todo)",1,NULL));
    json_append(a,row("Skills (view, create, edit)",1,NULL));
    return a;
}
