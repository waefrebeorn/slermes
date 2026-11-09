/*
 * browser_dom.h - browser DOM helpers.
 *
 * Module-internal header (browser.c <-> browser_dom.c).
 */
#ifndef BROWSER_DOM_H
#define BROWSER_DOM_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

void extract_title(const char *html, char *title, size_t title_sz);
char * html_to_text(const char *html);

#endif /* BROWSER_DOM_H */
