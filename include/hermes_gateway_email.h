/**
 * @file hermes_gateway_email.h
 * @brief Email platform (IMAP IDLE, SMTP, MIME, threading) declarations.
 */
#ifndef HERMES_GATEWAY_EMAIL_H
#define HERMES_GATEWAY_EMAIL_H

#include "hermes_gateway_types.h"
#include "hermes_http.h"

/* ================================================================
 *  P110: Email platform — IMAP IDLE, attachments, HTML, threading
 * ================================================================ */

void email_set_from(const char *from);

/* Basic send (backward compat) */
bool email_send_message(http_client_t *http, const char *to,
                        const char *subject, const char *body);

/* Extended send with HTML, attachments, and threading.
 * attachments: json array of {path, filename, mime_type} or NULL.
 * in_reply_to: Message-ID this email replies to, for threading (or NULL). */
bool email_send_message_ext(const char *to, const char *subject,
                             const char *text_body, const char *html_body,
                             json_node_t *attachments,
                             const char *in_reply_to);

/* IMAP IDLE push-based inbox monitoring.
 * Configure via env vars: EMAIL_IMAP_SERVER, EMAIL_IMAP_PORT,
 * EMAIL_IMAP_USER, EMAIL_IMAP_PASS, EMAIL_IMAP_MAILBOX. */
bool email_imap_init(void);    /* Load config from env vars */
void email_imap_start(void);   /* Start IDLE loop (blocks in thread) */
void email_imap_stop(void);    /* Signal IDLE loop to stop */

/* Legacy maildir polling (optional) */
json_node_t *email_poll_messages(http_client_t *http);

/* Update extractors */
const char *email_get_chat_id(json_node_t *update);
const char *email_get_text(json_node_t *update);

/* P110: Extended update extractors */
const char *email_get_html(json_node_t *update);        /* HTML body if present */
const char *email_get_subject(json_node_t *update);     /* Email subject */
json_node_t *email_get_attachments(json_node_t *update); /* Array of {filename, mime_type, path, size} */
const char *email_get_thread_id(json_node_t *update);   /* In-Reply-To (thread parent) */
const char *email_get_message_id(json_node_t *update);  /* This email's Message-ID */
const char *email_get_references(json_node_t *update);  /* References header for threading */

#endif /* HERMES_GATEWAY_EMAIL_H */