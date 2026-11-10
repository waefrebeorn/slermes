#ifndef PORT_GATEWAY_WHATSAPP_IDENTITY_H
#define PORT_GATEWAY_WHATSAPP_IDENTITY_H

/* Pure JID helpers of Python: gateway/whatsapp_identity.py.
 * handle `expand_whatsapp_aliases` reads lid-mapping files (runtime) and is
 * not ported. These three are pure string normalizations, oracle-verified. */

/* normalize_whatsapp_identifier: strip, drop one leading '+', then the text
 * before ':' and before '@'. malloc'd ("" for empty input -> empty str). */
char *wi_normalize_identifier(const char *value);

/* to_whatsapp_jid: bare phone -> <digits>@s.whatsapp.net; user:device@domain
 * -> user@domain; unchanged otherwise; "" for empty. malloc'd. */
char *wi_to_jid(const char *value);

/* normalize_whatsapp_mention_jid: the jid when user is all-ASCII-digits and
 * domain is s.whatsapp.net|lid, else "". malloc'd. */
char *wi_normalize_mention_jid(const char *value);

#endif /* PORT_GATEWAY_WHATSAPP_IDENTITY_H */
