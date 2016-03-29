#ifndef PERSISTENTS_H
#define PERSISTENTS_H

void load_persistents (environment_s *env);
void save_persistents (environment_s *env);
void save_colour_entry (css_colours_s *ety, GKeyFile *key_file);

#endif  /* PERSISTENTS_H */
