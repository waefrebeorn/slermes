#ifndef NOC_W20_UPDATECMDS_2CDFE8_H
#define NOC_W20_UPDATECMDS_2CDFE8_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w20_updatecmds_2cdfe8_stale_survivor_pids @ hermes_cli/update_cmd_stale_survivors.py:stale_fleet_survivor_pids
 * `{"status":"ok","value":[pids]}` for the live rows the fleet matrix stamped
 * `stale` that the restart phase never touched, or
 * `{"status":"raised","type":..,"message":..}` when a row is not a dict.
 * `already_signalled` is the restart receipt's killed-pid list (may be null). */
struct json_t *w20_updatecmds_2cdfe8_stale_survivor_pids(const struct json_t *fleet,
                                                         const struct json_t *already_signalled);

/* PoP: w20_updatecmds_2cdfe8_signal_stale_survivors @ hermes_cli/update_cmd_stale_survivors.py:signal_stale_fleet_survivors
 * Drain-first restart of every proven-stale gateway against a scripted
 * `restart` receipt: `{"status":"ok","value":{"signalled":[],"manual":[],
 * "killed":[],"stopped":[],"printed":[lines],"pending":[[pid,label,born,
 * supervised]]}}`, or the raised variant. */
struct json_t *w20_updatecmds_2cdfe8_signal_stale_survivors(const struct json_t *request);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W20_UPDATECMDS_2CDFE8_H */
