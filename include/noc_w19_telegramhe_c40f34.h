/* noc_w19_telegramhe_c40f34.h - public C API for plugins/platforms/telegram/telegram_held_inbound.py. */
#ifndef NOC_W19_TELEGRAMHE_C40F34_H
#define NOC_W19_TELEGRAMHE_C40F34_H
#include "hermes_json.h"
#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w19_telegramhe_c40f34_insert_by_arrival @ plugins/platforms/telegram/telegram_held_inbound.py:_insert_by_arrival */
json_t *w19_telegramhe_c40f34_insert_by_arrival(json_t *held_stamps, json_t *seq);
/* PoP: w19_telegramhe_c40f34_note_arrival @ plugins/platforms/telegram/telegram_held_inbound.py:_note_arrival */
json_t *w19_telegramhe_c40f34_note_arrival(json_t *existing_stamp, int counter);
/* PoP: w19_telegramhe_c40f34_hold_event @ plugins/platforms/telegram/telegram_held_inbound.py:_hold_inbound_event */
json_t *w19_telegramhe_c40f34_hold_event(json_t *queue, json_t *event, int fatal, int drop_delayed,
                                         int max_n, int schedule, json_t *successor_queue);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W19_TELEGRAMHE_C40F34_H */
