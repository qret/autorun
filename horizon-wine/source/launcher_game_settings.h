#ifndef AUTORUN_LAUNCHER_GAME_SETTINGS_H
#define AUTORUN_LAUNCHER_GAME_SETTINGS_H

#include "game_settings.h"
#include "launcher_ui.h"

struct launcher_game_settings
{
    const struct game_settings_profile *profile;
    struct game_settings *settings;
};

void launcher_game_settings_load( struct launcher_game_settings *state, const char *root,
                                  const char *target, uint16_t machine );
int launcher_game_settings_rows( const struct launcher_game_settings *state,
                                 struct ui_row *rows, int *ids, int group, int base );
void launcher_game_settings_action( struct launcher_game_settings *state, const char *root,
                                    struct ui *ui, struct ui_list *list, int id, enum ui_action action );

#endif
