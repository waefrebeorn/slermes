/* noc_w19_event_eea6c7.h - public C API for gateway/platforms/event.py port. */
#ifndef NOC_W19_EVENT_EEA6C7_H
#define NOC_W19_EVENT_EEA6C7_H
#include "hermes_json.h"
#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w19_event_eea6c7_clean_attachment_filename @ gateway/platforms/event.py:clean_attachment_filename */
json_t *w19_event_eea6c7_clean_attachment_filename(json_t *name);
/* PoP: w19_event_eea6c7_command_text @ gateway/platforms/event.py:MessageEvent._command_text */
json_t *w19_event_eea6c7_command_text(const char *text);
/* PoP: w19_event_eea6c7_is_command @ gateway/platforms/event.py:MessageEvent.is_command */
json_t *w19_event_eea6c7_is_command(const char *text, int allow_gateway_control);
/* PoP: w19_event_eea6c7_get_command @ gateway/platforms/event.py:MessageEvent.get_command */
json_t *w19_event_eea6c7_get_command(const char *text, int allow_gateway_control);
/* PoP: w19_event_eea6c7_get_command_args @ gateway/platforms/event.py:MessageEvent.get_command_args */
json_t *w19_event_eea6c7_get_command_args(const char *text, int allow_gateway_control);
/* PoP: w19_event_eea6c7_sender_id @ gateway/platforms/event.py:MessageEvent.sender_id */
json_t *w19_event_eea6c7_sender_id(json_t *user_id, json_t *source_user_id);
/* PoP: w19_event_eea6c7_absorb_reply_expected @ gateway/platforms/event.py:MessageEvent.absorb_reply_expected */
json_t *w19_event_eea6c7_absorb_reply_expected(json_t *self_reply_expected, json_t *other_reply_expected);
/* PoP: w19_event_eea6c7_absorb_turn_input @ gateway/platforms/event.py:MessageEvent.absorb_turn_input */
json_t *w19_event_eea6c7_absorb_turn_input(json_t *self_user_id, json_t *self_user_name,
                                          json_t *self_source_user_id, json_t *other_user_id,
                                          json_t *other_user_name, json_t *other_source_user_id,
                                          json_t *self_reply_expected, json_t *other_reply_expected);
/* PoP: w19_event_eea6c7_extend_media @ gateway/platforms/event.py:MessageEvent.extend_media */
json_t *w19_event_eea6c7_extend_media(json_t *self_urls, json_t *self_types, json_t *self_names,
                                      json_t *other_urls, json_t *other_types, json_t *other_names);
/* PoP: w19_event_eea6c7_accept_interrupt @ gateway/platforms/event.py:MessageEvent.accept_interrupt */
json_t *w19_event_eea6c7_accept_interrupt(json_t *events);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W19_EVENT_EEA6C7_H */
