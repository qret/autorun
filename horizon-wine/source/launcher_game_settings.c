#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>

#include "launcher.h"
#include "launcher_game_settings.h"

void launcher_game_settings_load( struct launcher_game_settings *state, const char *root,
                                  const char *target, uint16_t machine )
{
    const struct game_settings_profile *profile = game_settings_find( target, machine );

    if (profile == state->profile) return;
    game_settings_close( state->settings );
    state->profile = profile;
    state->settings = profile ? game_settings_open( root, profile ) : NULL;
}

static int choice_index( const struct game_setting *option, const char *value )
{
    for (unsigned int i = 0; i < option->choice_count; i++)
    {
        if (!strcasecmp( value, option->choices[i].value )) return i;
        if (option->type == GAME_SETTING_NUMBER || option->type == GAME_SETTING_INTEGER)
        {
            char *end;
            double number = strtod( value, &end );
            if (end != value && !*end && number == strtod( option->choices[i].value, NULL )) return i;
        }
    }
    return -1;
}

int launcher_game_settings_rows( const struct launcher_game_settings *state,
                                 struct ui_row *rows, int *ids, int group, int base )
{
    int count = 0, preset;

    if (!state->profile) return 0;
    memset( rows, 0, GAME_SETTINGS_MAX_ROWS * sizeof(*rows) );
    if (!state->settings)
    {
        snprintf( rows[0].label, sizeof(rows[0].label), "Cannot read game settings" );
        rows[0].help = "The existing INI was left unchanged. Select to try again.";
        rows[0].group = group;
        ids[0] = base;
        return 1;
    }
    preset = game_settings_current_preset( state->settings );
    snprintf( rows[count].label, sizeof(rows[count].label), "Graphics preset" );
    snprintf( rows[count].value, sizeof(rows[count].value), "%s", preset < 0 ? "Custom" : state->profile->presets[preset] );
    rows[count].kind = UI_ROW_DROPDOWN;
    rows[count].choices = state->profile->preset_count;
    rows[count].help = "Edits the game's INI. Applied once; later edits are preserved. Saves and controls are unchanged.";
    ids[count++] = base;
    for (unsigned int i = 0; i < state->profile->count && count < GAME_SETTINGS_MAX_ROWS; i++)
    {
        const struct game_setting *option = state->profile->settings + i;
        char value[GAME_SETTINGS_VALUE_SIZE];
        int choice;
        struct ui_row *row = rows + count;

        snprintf( row->label, sizeof(row->label), "%s", option->label );
        row->help = option->key;
        row->kind = option->choice_count ? UI_ROW_DROPDOWN : UI_ROW_VALUE;
        row->choices = option->choice_count;
        if (!game_settings_get( state->settings, i, value, sizeof(value) ))
            snprintf( row->value, sizeof(row->value), "Game default" );
        else
        {
            choice = choice_index( option, value );
            snprintf( row->value, sizeof(row->value), "%s", choice < 0 ? value : option->choices[choice].label );
        }
        ids[count++] = base + 1 + i;
    }
    for (int i = 0; i < count; i++) rows[i].group = group;
    return count;
}

void launcher_game_settings_action( struct launcher_game_settings *state, const char *root,
                                    struct ui *ui, struct ui_list *list, int id, enum ui_action action )
{
    int ok = 0;
    if (action != UI_ACTION_CHOOSE && action != UI_ACTION_RESET) return;
    if (!state->settings)
    {
        state->settings = game_settings_open( root, state->profile );
        if (!state->settings) ui_message( ui, "Game Settings", "The game settings could not be read. The existing file was left unchanged." );
        return;
    }
    if (!id)
    {
        struct ui_row items[4] = {0};
        int selected = 0;

        if (action == UI_ACTION_CHOOSE)
        {
            for (unsigned int i = 0; i < state->profile->preset_count; i++)
                snprintf( items[i].label, sizeof(items[i].label), "%s", state->profile->presets[i] );
            selected = ui_settings_dropdown( ui, list, items, state->profile->preset_count,
                                              game_settings_current_preset( state->settings ) );
            if (selected < 0) return;
        }
        if (!ui_confirm( ui, "Graphics preset", "Replace the current graphics settings with this preset?", "Apply" )) return;
        ok = game_settings_apply_preset( state->settings, selected );
    }
    else if (id >= 1 && (unsigned int)(id - 1) < state->profile->count)
    {
        const struct game_setting *option = state->profile->settings + id - 1;
        char current[GAME_SETTINGS_VALUE_SIZE] = "", value[GAME_SETTINGS_VALUE_SIZE];
        const char *chosen;

        game_settings_get( state->settings, id - 1, current, sizeof(current) );
        if (action == UI_ACTION_RESET) chosen = option->initial ? option->initial : option->presets[0];
        else if (option->choice_count)
        {
            struct ui_row *items = calloc( option->choice_count, sizeof(*items) );
            int selected;
            if (!items) return;
            for (unsigned int i = 0; i < option->choice_count; i++)
                snprintf( items[i].label, sizeof(items[i].label), "%s", option->choices[i].label );
            selected = ui_settings_dropdown( ui, list, items, option->choice_count, choice_index( option, current ) );
            free( items );
            if (selected < 0) return;
            chosen = option->choices[selected].value;
        }
        else
        {
            if (!launcher_platform_prompt( option->label, current, value, sizeof(value) )) return;
            chosen = value;
        }
        if (!chosen) return;
        ok = game_settings_set( state->settings, id - 1, chosen );
    }
    if (!ok || !game_settings_save( state->settings ))
    {
        game_settings_close( state->settings );
        state->settings = game_settings_open( root, state->profile );
        ui_message( ui, "Game Settings", ok ? "The game settings could not be saved." : "The value could not be applied. Check the number and try again." );
    }
}
