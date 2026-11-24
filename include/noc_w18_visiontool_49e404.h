#ifndef NOC_W18_VISIONTOOL_49E404_H
#define NOC_W18_VISIONTOOL_49E404_H

/* C home for upstream Hermes tools/vision_tools_image_prep.py.
 * Both functions return a freshly malloc'd string (caller frees) or NULL,
 * mirroring Python's Optional[str] return. */
const char *w18_visiontool_49e404_unsupported_inline_image_media_type(const char *url);
const char *w18_visiontool_49e404_detect_image_mime_type_from_bytes(const char *data_hex,
                                                                    int png_verify_ok);

#endif /* NOC_W18_VISIONTOOL_49E404_H */
