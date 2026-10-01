#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#include "game_settings.h"
#include "games/fallout3.h"
#include "games/fallout4.h"
#include "games/moha.h"
#include "games/spiderman_wos.h"

#define INI_LIMIT 65536

struct game_settings
{
    const struct game_settings_profile *profile;
    char path[1024];
    char text[INI_LIMIT + 1];
    size_t size;
    int dirty;
};

static int span_equal( const char *text, size_t begin, size_t end, const char *value )
{
    while (begin < end && isspace( (unsigned char)text[begin] )) begin++;
    while (end > begin && isspace( (unsigned char)text[end - 1] )) end--;
    return end - begin == strlen( value ) && !strncasecmp( text + begin, value, end - begin );
}

static int ini_find( const struct game_settings *settings, const char *section, const char *key,
                     size_t *begin, size_t *end, size_t *value, size_t *insert )
{
    size_t pos = 0;
    int match = 0;

    *insert = SIZE_MAX;
    while (pos < settings->size)
    {
        size_t start = pos, p, stop;
        const char *equal, *bracket;

        while (pos < settings->size && settings->text[pos] != '\n') pos++;
        stop = pos;
        if (pos < settings->size) pos++;
        p = start;
        if (!p && stop >= 3 && !memcmp( settings->text, "\xef\xbb\xbf", 3 )) p += 3;
        while (p < stop && isspace( (unsigned char)settings->text[p] )) p++;
        if (p == stop || settings->text[p] == ';' || settings->text[p] == '#') continue;
        if (settings->text[p] == '[')
        {
            bracket = memchr( settings->text + p, ']', stop - p );
            match = bracket && span_equal( settings->text, p + 1, bracket - settings->text, section );
        }
        else if (match && (equal = memchr( settings->text + p, '=', stop - p )) &&
                 span_equal( settings->text, p, equal - settings->text, key ))
        {
            *begin = start;
            *end = pos;
            *value = equal - settings->text + 1;
            return 1;
        }
        if (match) *insert = pos;
    }
    return 0;
}

static int ini_get( const struct game_settings *settings, const char *section, const char *key,
                    char *out, size_t size )
{
    size_t begin, end, value, insert;

    if (!size) return 0;
    *out = 0;
    if (!ini_find( settings, section, key, &begin, &end, &value, &insert )) return 0;
    while (value < end && isspace( (unsigned char)settings->text[value] )) value++;
    while (end > value && isspace( (unsigned char)settings->text[end - 1] )) end--;
    if (end - value >= size) return 0;
    memcpy( out, settings->text + value, end - value );
    out[end - value] = 0;
    return 1;
}

static int ini_replace( struct game_settings *settings, size_t begin, size_t end, const char *text )
{
    size_t length = strlen( text );

    if (length > INI_LIMIT - settings->size + (end - begin)) return 0;
    memmove( settings->text + begin + length, settings->text + end, settings->size - end + 1 );
    memcpy( settings->text + begin, text, length );
    settings->size = settings->size - (end - begin) + length;
    settings->dirty = 1;
    return 1;
}

static int ini_set( struct game_settings *settings, const char *section, const char *key, const char *value )
{
    size_t begin, end, old_value, insert;
    char line[256];
    int n;

    /* Collapse duplicate keys, including repeated sections. */
    while (ini_find( settings, section, key, &begin, &end, &old_value, &insert ))
    {
        if (!ini_replace( settings, begin, end, "" )) return 0;
    }
    ini_find( settings, section, key, &begin, &end, &old_value, &insert );
    if (insert == SIZE_MAX)
    {
        insert = settings->size;
        n = snprintf( line, sizeof(line), "%s[%s]\r\n",
                      insert && settings->text[insert - 1] != '\n' ? "\r\n" : "", section );
        if (n < 0 || (size_t)n >= sizeof(line) || !ini_replace( settings, insert, insert, line )) return 0;
        insert = settings->size;
    }
    n = snprintf( line, sizeof(line), "%s%s=%s\r\n",
                  insert && settings->text[insert - 1] != '\n' ? "\r\n" : "", key, value );
    return n >= 0 && (size_t)n < sizeof(line) && ini_replace( settings, insert, insert, line );
}

static const char *xml_skip( const char *p )
{
    for (;;)
    {
        while (isspace( (unsigned char)*p )) p++;
        if (!strncmp( p, "<!--", 4 ))
        {
            if (!(p = strstr( p + 4, "-->" ))) return NULL;
            p += 3;
        }
        else if (!strncmp( p, "<?", 2 ))
        {
            if (!(p = strstr( p + 2, "?>" ))) return NULL;
            p += 2;
        }
        else return p;
    }
}

/* AWL stores settings as flat /r/s[@id] entries. */
static int xml_find( const struct game_settings *settings, const char *key,
                     size_t *begin, size_t *end, size_t *value, size_t *value_end, size_t *insert )
{
    const char *p = settings->text, *start, *id, *stop;
    size_t id_length;
    int found = 0;

    if (!strncmp( p, "\xef\xbb\xbf", 3 )) p += 3;
    if (!(p = xml_skip( p )) || strncmp( p, "<r", 2 )) return -1;
    p += 2;
    while (isspace( (unsigned char)*p )) p++;
    if (*p++ != '>') return -1;
    for (;;)
    {
        if (!(p = xml_skip( p ))) return -1;
        if (!strncmp( p, "</r", 3 ))
        {
            *insert = p - settings->text;
            p += 3;
            while (isspace( (unsigned char)*p )) p++;
            if (*p++ != '>' || !(p = xml_skip( p )) || *p) return -1;
            return found;
        }
        start = p;
        if (strncmp( p, "<s", 2 ) || !isspace( (unsigned char)p[2] )) return -1;
        p += 2;
        id = NULL;
        id_length = 0;
        while (*p)
        {
            const char *attribute, *text;
            size_t length;
            char quote;

            while (isspace( (unsigned char)*p )) p++;
            if (*p == '>' || *p == '/') break;
            attribute = p;
            while (isalnum( (unsigned char)*p ) || *p == '_' || *p == ':' || *p == '-') p++;
            length = p - attribute;
            if (!length) return -1;
            while (isspace( (unsigned char)*p )) p++;
            if (*p++ != '=') return -1;
            while (isspace( (unsigned char)*p )) p++;
            quote = *p++;
            if (quote != '\'' && quote != '"') return -1;
            text = p;
            if (!(p = strchr( p, quote ))) return -1;
            if (length == 2 && !memcmp( attribute, "id", 2 ))
            {
                if (id) return -1;
                id = text;
                id_length = p - text;
            }
            p++;
        }
        if (!id) return -1;
        if (*p == '/')
        {
            if (*++p != '>') return -1;
            stop = ++p;
        }
        else
        {
            if (*p++ != '>') return -1;
            stop = strchr( p, '<' );
            if (!stop || strncmp( stop, "</s", 3 )) return -1;
            const char *close = stop + 3;
            while (isspace( (unsigned char)*close )) close++;
            if (*close++ != '>') return -1;
            if (!found && key && strlen(key) == id_length && !memcmp( id, key, id_length ))
            {
                *value = p - settings->text;
                *value_end = stop - settings->text;
            }
            p = close;
        }
        if (!found && key && strlen(key) == id_length && !memcmp( id, key, id_length ))
        {
            const char *after = p;
            while (start > settings->text && (start[-1] == ' ' || start[-1] == '\t')) start--;
            while (*after == ' ' || *after == '\t') after++;
            if (*after == '\r') after++;
            if (*after == '\n') after++;
            *begin = start - settings->text;
            *end = after - settings->text;
            if (stop == p) *value = *value_end = p - settings->text;
            found = 1;
        }
    }
}

static int config_get( const struct game_settings *settings, const char *section, const char *key,
                       char *out, size_t size )
{
    size_t begin, end, value, value_end, insert;
    char id[128];
    int n;

    if (settings->profile->format == GAME_SETTINGS_INI) return ini_get( settings, section, key, out, size );
    n = snprintf( id, sizeof(id), "%s%s%s", section ? section : "", section && *section ? "." : "", key );
    if (n < 0 || (size_t)n >= sizeof(id) || !size) return 0;
    *out = 0;
    if (xml_find( settings, id, &begin, &end, &value, &value_end, &insert ) != 1) return 0;
    while (value < value_end && isspace( (unsigned char)settings->text[value] )) value++;
    while (value_end > value && isspace( (unsigned char)settings->text[value_end - 1] )) value_end--;
    if (value_end - value >= size) return 0;
    memcpy( out, settings->text + value, value_end - value );
    out[value_end - value] = 0;
    return 1;
}

static int config_set( struct game_settings *settings, const char *section, const char *key, const char *text )
{
    size_t begin, end, value, value_end, insert;
    char id[128], line[256];
    int n, found;

    if (settings->profile->format == GAME_SETTINGS_INI) return ini_set( settings, section, key, text );
    n = snprintf( id, sizeof(id), "%s%s%s", section ? section : "", section && *section ? "." : "", key );
    if (n < 0 || (size_t)n >= sizeof(id) || strpbrk( id, "<&>\"'" ) || strpbrk( text, "<&>" )) return 0;
    while ((found = xml_find( settings, id, &begin, &end, &value, &value_end, &insert )) == 1)
        if (!ini_replace( settings, begin, end, "" )) return 0;
    if (found < 0) return 0;
    n = snprintf( line, sizeof(line), "  <s id=\"%s\">%s</s>\n", id, text );
    return n >= 0 && (size_t)n < sizeof(line) && ini_replace( settings, insert, insert, line );
}

const struct game_settings_profile *game_settings_find( const char *target, uint16_t machine )
{
    static const struct game_settings_profile *const profiles[] = {
        &fallout3_profile, &fallout4_profile, &moha_profile, &spiderman_wos_profile
    };
    const char *name = target;

    for (const char *p = target; *p; p++) if (*p == '/' || *p == '\\') name = p + 1;
    for (unsigned int i = 0; i < sizeof(profiles) / sizeof(profiles[0]); i++)
        if (profiles[i]->machine == machine && !strcasecmp( name, profiles[i]->executable )) return profiles[i];
    return NULL;
}

int game_settings_append_arguments( const char *target, uint16_t machine, char *command_line, size_t size )
{
    const struct game_settings_profile *profile = game_settings_find( target, machine );
    size_t length, extra;

    if (!profile || !profile->arguments) return 1;
    length = strnlen( command_line, size );
    extra = strlen( profile->arguments );
    if (length >= size || size - length <= extra + 1) return 0;
    command_line[length] = ' ';
    memcpy( command_line + length + 1, profile->arguments, extra + 1 );
    return 1;
}

struct game_settings *game_settings_open( const char *root, const struct game_settings_profile *profile )
{
    struct game_settings *settings;
    char marker[32], backup[1100];
    FILE *file;
    int ok = 1;

    if (!profile || profile->preset_count > 4 || profile->format > GAME_SETTINGS_XML ||
        profile->count > GAME_SETTINGS_MAX_ROWS - 1) return NULL;
    if (!(settings = calloc( 1, sizeof(*settings) ))) return NULL;
    settings->profile = profile;
    if ((size_t)snprintf( settings->path, sizeof(settings->path), "%s/%s", root, profile->path ) >= sizeof(settings->path))
        goto failed;
    snprintf( backup, sizeof(backup), "%s.autorun-old", settings->path );
    if (access( settings->path, F_OK ) && errno == ENOENT && !access( backup, F_OK ))
        if (rename( backup, settings->path )) goto failed;
    file = fopen( settings->path, "rb" );
    if (!file && errno != ENOENT) goto failed;
    if (file)
    {
        settings->size = fread( settings->text, 1, INI_LIMIT, file );
        ok = !ferror( file ) && fgetc( file ) == EOF;
        if (fclose( file )) ok = 0;
        if (!ok || memchr( settings->text, 0, settings->size ) ||
            (settings->size >= 2 && ((unsigned char)settings->text[0] == 0xff ||
                                     (unsigned char)settings->text[0] == 0xfe))) goto failed;
    }
    if (!settings->size && profile->initial_text)
    {
        settings->size = strlen( profile->initial_text );
        if (settings->size > INI_LIMIT) goto failed;
        memcpy( settings->text, profile->initial_text, settings->size + 1 );
        settings->dirty = 1;
    }
    if (profile->format == GAME_SETTINGS_XML)
    {
        size_t begin, end, value, value_end, insert;
        if (xml_find( settings, NULL, &begin, &end, &value, &value_end, &insert ) < 0) goto failed;
    }
    if (!config_get( settings, "Autorun", "SettingsApplied", marker, sizeof(marker) ) || strcmp( marker, "1" ))
    {
        if (profile->preset_count && !game_settings_apply_preset( settings, 0 )) goto failed;
        for (unsigned int i = 0; i < profile->count; i++)
            if (profile->settings[i].initial &&
                !game_settings_set( settings, i, profile->settings[i].initial )) goto failed;
        if (!config_set( settings, "Autorun", "SettingsApplied", "1" )) goto failed;
    }
    return settings;
failed:
    free( settings );
    return NULL;
}

void game_settings_close( struct game_settings *settings )
{
    free( settings );
}

int game_settings_get( const struct game_settings *settings, unsigned int index, char *value, size_t size )
{
    const struct game_setting *option;
    char first[GAME_SETTINGS_VALUE_SIZE], second[GAME_SETTINGS_VALUE_SIZE];
    int n;

    if (index >= settings->profile->count) return 0;
    option = settings->profile->settings + index;
    if (!config_get( settings, option->section, option->key, first, sizeof(first) )) return 0;
    if (option->type != GAME_SETTING_RESOLUTION) n = snprintf( value, size, "%s", first );
    else
    {
        if (!config_get( settings, option->section, option->key2, second, sizeof(second) )) return 0;
        n = snprintf( value, size, "%sx%s", first, second );
    }
    return n >= 0 && (size_t)n < size;
}

int game_settings_set( struct game_settings *settings, unsigned int index, const char *value )
{
    const struct game_setting *option;
    char *end;

    if (index >= settings->profile->count) return 0;
    option = settings->profile->settings + index;
    if (strlen( value ) >= GAME_SETTINGS_VALUE_SIZE || strpbrk( value, "\r\n" )) return 0;
    if (option->type == GAME_SETTING_RESOLUTION)
    {
        unsigned int width, height;
        char extra, first[16], second[16];

        if (sscanf( value, "%ux%u%c", &width, &height, &extra ) != 2 ||
            width < 320 || width > 7680 || height < 200 || height > 4320) return 0;
        snprintf( first, sizeof(first), "%u", width );
        snprintf( second, sizeof(second), "%u", height );
        if (!config_set( settings, option->section, option->key, first ) ||
            !config_set( settings, option->section, option->key2, second )) return 0;
        for (unsigned int i = 0; i < 2; i++)
            if (settings->profile->pending_resolution[i] &&
                !config_set( settings, option->section, settings->profile->pending_resolution[i], "0" )) return 0;
        return 1;
    }
    if (option->type == GAME_SETTING_NUMBER || option->type == GAME_SETTING_INTEGER)
    {
        double number = strtod( value, &end );
        if (end == value || *end || !isfinite( number ) || number < 0 || number > 10000000) return 0;
        if (option->type == GAME_SETTING_INTEGER && number != (unsigned int)number) return 0;
    }
    return config_set( settings, option->section, option->key, value );
}

int game_settings_apply_preset( struct game_settings *settings, int preset )
{
    if (preset < 0 || (unsigned int)preset >= settings->profile->preset_count) return 0;
    for (unsigned int i = 0; i < settings->profile->count; i++)
    {
        const struct game_setting *option = settings->profile->settings + i;
        const char *value = option->presets[preset];
        if (value && !game_settings_set( settings, i, value )) return 0;
    }
    return 1;
}

int game_settings_current_preset( const struct game_settings *settings )
{
    for (unsigned int preset = 0; preset < settings->profile->preset_count; preset++)
    {
        unsigned int i;
        for (i = 0; i < settings->profile->count; i++)
        {
            const struct game_setting *option = settings->profile->settings + i;
            const char *expected = option->presets[preset];
            char value[GAME_SETTINGS_VALUE_SIZE];
            if (!expected) continue;
            if (!game_settings_get( settings, i, value, sizeof(value) )) break;
            if (option->type == GAME_SETTING_NUMBER || option->type == GAME_SETTING_INTEGER)
            {
                char *end;
                double number = strtod( value, &end );
                if (end == value || *end || number != strtod( expected, NULL )) break;
            }
            else if (strcasecmp( value, expected )) break;
        }
        if (i == settings->profile->count) return preset;
    }
    return -1;
}

int game_settings_save( struct game_settings *settings )
{
    char path[1024], temp[1100], backup[1100];
    FILE *file;
    int ok, moved = 0;

    if (!settings->dirty) return 1;
    snprintf( path, sizeof(path), "%s", settings->path );
    for (char *p = path + 1; *p; p++) if (*p == '/' && p[-1] != ':')
    {
        *p = 0;
        if (mkdir( path, 0777 ) && errno != EEXIST) return 0;
        *p = '/';
    }
    snprintf( temp, sizeof(temp), "%s.autorun-new", path );
    snprintf( backup, sizeof(backup), "%s.autorun-old", path );
    if (!(file = fopen( temp, "wb" ))) return 0;
    ok = fwrite( settings->text, 1, settings->size, file ) == settings->size;
    if (ok && (fflush( file ) || fsync( fileno( file ) ))) ok = 0;
    if (fclose( file )) ok = 0;
    if (!ok) { remove( temp ); return 0; }
    if (!access( path, F_OK ))
    {
        if ((remove( backup ) && errno != ENOENT) || rename( path, backup ))
        { remove( temp ); return 0; }
        moved = 1;
    }
    if (rename( temp, path ))
    {
        if (moved) rename( backup, path );
        remove( temp );
        return 0;
    }
    if (moved) remove( backup );
    settings->dirty = 0;
    return 1;
}

int game_settings_prepare( const char *root, const char *target, uint16_t machine )
{
    const struct game_settings_profile *profile = game_settings_find( target, machine );
    struct game_settings *settings;
    int ok;

    if (!profile) return 1;
    if (!(settings = game_settings_open( root, profile ))) return 0;
    ok = game_settings_save( settings );
    game_settings_close( settings );
    return ok;
}
