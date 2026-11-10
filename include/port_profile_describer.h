/*
 * port_profile_describer.h — pure helper from hermes_cli/profile_describer.py
 * _sample_skills. The filesystem skill discovery (_collect_skills) stays
 * unported.
 */
#ifndef PORT_PROFILE_DESCRIBER_H
#define PORT_PROFILE_DESCRIBER_H

/* _sample_skills(names): the profile probe prompt's skill sample. Returns the
 * list unchanged when len<=60, else 60 evenly-spaced picks
 * [names[int(i*step)] for i in 0..59], step = len/60.0. Input is a JSON array
 * of strings; returns malloc'd JSON, caller frees. */
char *pd_sample_skills(const char *names_json);

#endif /* PORT_PROFILE_DESCRIBER_H */
