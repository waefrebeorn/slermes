#ifndef NOC_W18_EARLYRECO_D648F9_H
#define NOC_W18_EARLYRECO_D648F9_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w18_earlyreco_d648f9_parse_zip_swap_journal @ hermes_cli/_early_recovery_zip.py:_parse_zip_swap_journal
 * {phase,gen,entries,temps} object from a valid ZIP swap journal, else JSON null. */
struct json_t *w18_earlyreco_d648f9_parse_zip_swap_journal(const char *raw);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W18_EARLYRECO_D648F9_H */
