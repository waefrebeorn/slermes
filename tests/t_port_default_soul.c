/* Oracle harness: hermes_cli/default_soul.py vs LIVE Python. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cli/port_default_soul.h"

static const char *jstr(const char *s){
    static char b[4][4096]; static int bi=0; int idx=bi; char *q=b[idx]; bi=(bi+1)&3; *q++='"';
    for(const char *p=s;p&&*p&&(q-b[idx])<4080;p++){
        unsigned char c=*p;
        if(c=='"'||c=='\\'){*q++='\\';*q++=c;}
        else if(c<0x20){*q++='\\';*q++='u';*q++='0';*q++='0';*q++="0123456789abcdef"[c>>4];*q++="0123456789abcdef"[c&0xf];}
        else *q++=c;
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
        {NULL, "legacy2"},   /* filled below from the live DEFAULT_SOUL_MD-era text */
        {NULL, "legacy3"},
    };
    unsigned nc = sizeof(cases)/sizeof(cases[0]);
    /* the second-generation default persona (template index 2) */
    cases[6].t =
        "You are Hermes Agent, an intelligent AI assistant created by Nous Research. You are helpful, "
        "knowledgeable, and direct. You assist users with a wide range of tasks including answering questions, "
        "writing and editing code, analyzing information, creative work, and executing actions via your tools. "
        "You communicate clearly, admit uncertainty when appropriate, and prioritize being genuinely useful over "
        "being verbose unless otherwise directed below. Be targeted and efficient in your exploration and "
        "investigations.";
    /* the ASCII-dashed install variant (template index 3) */
    cases[7].t =
        "You are Hermes Agent, built by Nous Research. Be direct: match the length of your reply to the "
        "weight of the ask -- a one-line question gets a one-line answer, and finished work gets a short "
        "report of what changed, what's verified, and what's left, never a replay of the process. No filler "
        "(\"Great question,\" \"I'd be happy to\"), no restating the request back, no re-summarizing what you "
        "already said, no narrating tool calls the user can see. Plain claims over adjectives; when unsure, "
        "say so plainly. Agree because it's right, not because the user said it. Depth is earned -- give it "
        "when the user asks for detail, teaches, or the stakes demand it, not by default.";
    for (int i = 0; i < (int)nc; i++) {
        bool leg = hermes_cli_default_soul_is_legacy(cases[i].t);
        printf("{\"t\":\"legacy\",\"name\":%s,\"in\":%s,\"out\":%d}\n",
               jstr(cases[i].name), jstr(cases[i].t), leg?1:0);
    }
    free(wsc); free(bomc);
    return 0;
}
