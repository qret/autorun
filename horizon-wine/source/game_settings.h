#ifndef AUTORUN_GAME_SETTINGS_H
#define AUTORUN_GAME_SETTINGS_H

#include <stddef.h>
#include <stdint.h>

#define GAME_SETTINGS_MAX_ROWS 64
#define GAME_SETTINGS_VALUE_SIZE 64

enum game_setting_type { GAME_SETTING_NUMBER, GAME_SETTING_INTEGER, GAME_SETTING_TEXT, GAME_SETTING_RESOLUTION };

struct game_setting_choice
{
    const char *label, *value;
};

struct game_setting
{
    const char *section, *key, *key2, *label;
    const char *presets[4];
    const char *initial;
    const struct game_setting_choice *choices;
    unsigned int choice_count;
    enum game_setting_type type;
};

struct game_settings_profile
{
    const char *executable;
    uint16_t machine;
    const char *path;
    const char *const *presets;
    unsigned int preset_count;
    const struct game_setting *settings;
    unsigned int count;
};

struct game_settings;

const struct game_settings_profile *game_settings_find( const char *target, uint16_t machine );
struct game_settings *game_settings_open( const char *root, const struct game_settings_profile *profile );
void game_settings_close( struct game_settings *settings );
int game_settings_get( const struct game_settings *settings, unsigned int index, char *value, size_t size );
int game_settings_set( struct game_settings *settings, unsigned int index, const char *value );
int game_settings_apply_preset( struct game_settings *settings, int preset );
int game_settings_current_preset( const struct game_settings *settings );
int game_settings_save( struct game_settings *settings );
int game_settings_prepare( const char *root, const char *target, uint16_t machine );

#endif
