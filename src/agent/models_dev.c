/*
 * models_dev.c — Port of Python agent/models_dev.py
 *
 * Python API → C implementation mapping:
 *   model_metadata_lookup_dev()  → model_metadata_lookup_dev() in provider_metadata.c
 *   model_is_dev_model()         → model_is_dev_model() in provider_metadata.c
 *   register_dev_model()         → N/A (dev models hardcoded in provider_metadata.c PROVIDERS[])
 *   list_dev_models()            → N/A (CLI-level feature)
 *
 * Developer model configuration — ported inside provider_metadata.c
 * as part of the PROVIDERS[] static array.
 */

#include "provider_metadata.h"   /* model_metadata_lookup_dev(), model_is_dev_model() */
