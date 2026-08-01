/* Oracle harness: hermes_cli/default_soul.py vs LIVE Python. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cli/port_default_soul.h"

static const char *jstr(const char *s){
    static char b[4][1024]; static int bi=0; int idx=bi; char *q=b[idx]; bi=(bi+1)&3; *q++='"';
    for(const char *p=s;p&&*p&&(q-b[idx])<500;p++){
        unsigned char c=*p;
        if(c=='"'||c=='\\'){*q++='\\';*q++=c;} else *q++=c;
    }
    *q++='"';*q='\0';return b[idx];
}

static const char *LEGACY0 =
    "# Hermes Agent Persona\n"
    "\n"
    "<!--\n"
    "This file defines the agent's personality and tone.\n"
    "The agent will embody whatever you write here.\n"
    "Edit this to customize how Hermes communicates with you.\n"
    "\n"
    "Examples:\n"
    "  - \"You are a warm, playful assistant who uses kaomoji occasionally.\"\n"
    "  - \"You are a concise technical expert. No fluff, just facts.\"\n"
    "  - \"You speak like a friendly coworker who happens to know everything.\"\n"
    "\n"
    "This file is loaded fresh each message -- no restart needed.\n"
    "Delete the contents (or this file) to use the default personality.\n"
    "-->";
static const char *LEGACY1 =
    "# Hermes Agent Persona\n"
    "\n"
    "<!--\n"
    "This file defines the agent's personality and tone.\n"
    "The agent will embody whatever you write here.\n"
    "Edit this to customize how Hermes communicates with you.\n"
    "\n"
    "This file is loaded fresh each message -- no restart needed.\n"
    "Delete the contents (or this file) to use the default personality.\n"
    "-->";

int main(void) {
    const char *custom = "You are a helpful assistant with a custom persona.";
    const char *ws = "  \r\n"; char *wsc = malloc(strlen(ws)+strlen(LEGACY0)+strlen("\r\n  ")+1);
    sprintf(wsc, "%s%s%s", ws, LEGACY0, "\r\n  ");
    const char *bom = "\xef\xbb\xbf"; char *bomc = malloc(3+strlen(LEGACY0)+1+1);
    sprintf(bomc, "%s%s\n", bom, LEGACY0);

    struct { const char *t; const char *name; } cases[] = {
        {LEGACY0, "legacy0"},
        {LEGACY1, "legacy1"},
        {custom, "custom"},
        {wsc, "legacy0_ws"},
        {bomc, "legacy0_bom"},
        {"", "empty"},
    };
    for (int i = 0; i < 6; i++) {
        bool leg = hermes_cli_default_soul_is_legacy(cases[i].t);
        printf("{\"t\":\"legacy\",\"name\":%s,\"out\":%d}\n", jstr(cases[i].name), leg?1:0);
    }
    free(wsc); free(bomc);
    return 0;
}
