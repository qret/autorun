#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int fail_sync, fail_install, writes;
static int test_fsync( int fd )
{
    writes++;
    if (fail_sync) { errno = EIO; return -1; }
    return fsync( fd );
}
static int test_rename( const char *from, const char *to )
{
    if (fail_install && strstr( from, ".autorun-new" )) { errno = EIO; return -1; }
    /* Horizon's FAT devoptab does not replace existing files. */
    if (!access( to, F_OK )) { errno = EEXIST; return -1; }
    return rename( from, to );
}
#define fsync test_fsync
#define rename test_rename
#include "../source/game_settings.c"
#undef fsync
#undef rename

static unsigned int profile_option_index( const struct game_settings_profile *profile, const char *key )
{
    for (unsigned int i = 0; i < profile->count; i++)
        if (!strcmp( profile->settings[i].key, key )) return i;
    abort();
}

static unsigned int option_index( const char *key )
{
    return profile_option_index( &fallout4_profile, key );
}

static void expect( struct game_settings *settings, const char *key, const char *expected )
{
    char value[64];
    assert( game_settings_get( settings, option_index( key ), value, sizeof(value) ) );
    assert( !strcmp( value, expected ) );
}

static void write_text( const char *path, const char *text )
{
    FILE *file = fopen( path, "wb" );
    assert( file );
    assert( fwrite( text, 1, strlen( text ), file ) == strlen( text ) );
    assert( !fclose( file ) );
}

static void check_spiderman_wos_settings(void)
{
    char directory[] = "/tmp/spiderman-settings-XXXXXX", path[1024], text[2048], oversized[1100];
    const struct game_settings_profile *profile = &spiderman_wos_profile;
    struct game_settings *settings;
    FILE *file;
    int previous_writes;

    assert( mkdtemp( directory ) );
    snprintf( path, sizeof(path), "%s/%s", directory, profile->path );
    assert( game_settings_find( "ums0:/Games/Spider-Man Web of Shadows.exe", 0x014c ) == profile );
    assert( !game_settings_find( "Spider-Man Launcher.exe", 0x014c ) );
    assert( !game_settings_find( "Spider-Man Web of Shadows.exe", 0x8664 ) );
    assert( game_settings_prepare( directory, "Spider-Man Launcher.exe", 0x014c ) );
    assert( game_settings_prepare( directory, "Spider-Man Web of Shadows.exe", 0x8664 ) );
    assert( game_settings_prepare( directory, "Spider-Man Web of Shadows.exe.bak", 0x014c ) );
    assert( access( path, F_OK ) );
    fail_sync = 1;
    assert( !game_settings_prepare( directory, "ums0:/Games/Spider-Man Web of Shadows.exe", 0x014c ) );
    assert( access( path, F_OK ) );
    fail_sync = 0;
    assert( game_settings_prepare( directory, "sdmc:/Games/Spider-Man Web of Shadows.exe", 0x014c ) );
    file = fopen( path, "rb" );
    assert( file );
    size_t size = fread( text, 1, sizeof(text) - 1, file );
    text[size] = 0;
    assert( strstr( text, "<s id=\"VideoW\">1280</s>" ) );
    assert( strstr( text, "<s id=\"VideoH\">720</s>" ) );
    assert( strstr( text, "<s id=\"VideoDesiredW\">0</s>" ) );
    assert( strstr( text, "<s id=\"VideoDesiredH\">0</s>" ) );
    assert( strstr( text, "<s id=\"ActionCenterCamera\">526</s>" ) );
    assert( !fclose( file ) );
    write_text( path, "\xef\xbb\xbf<?xml version='1.0'?>\n<r>\n"
                "<!-- <s id='VideoW'>999</s> -->\n"
                "<s other='keep' id = 'VideoW'>800</s><s id='VideoW'>640</s>\n"
                "<s id='VideoH'>600</s><s id='Version'>7</s><s id='Mod'>A&amp;B</s>\n"
                "<s id='VideoDesiredW'>1920</s><s id='VideoDesiredH'>1080</s>\n</r>\n" );
    assert( game_settings_prepare( directory, "Spider-Man Web of Shadows.exe", 0x014c ) );
    settings = game_settings_open( directory, profile );
    assert( settings && game_settings_current_preset( settings ) == -1 );
    assert( game_settings_get( settings, 0, text, sizeof(text) ) && !strcmp( text, "1280x720" ) );
    assert( strstr( settings->text, "<s id='Mod'>A&amp;B</s>" ) );
    assert( strstr( settings->text, "<!-- <s id='VideoW'>999</s> -->" ) );
    assert( !strstr( settings->text, "id = 'VideoW'" ) && !strstr( settings->text, "<s id='VideoW'>640</s>" ) );
    assert( !memcmp( settings->text, "\xef\xbb\xbf", 3 ) );
    assert( !game_settings_set( settings, 0, "1280x0" ) );
    assert( game_settings_set( settings, 0, "1920x1080" ) );
    assert( game_settings_set( settings, profile_option_index( profile, "VideoShadows" ), "0" ) );
    assert( game_settings_set( settings, profile_option_index( profile, "MouseSens" ), "37.5" ) );
    assert( game_settings_set( settings, profile_option_index( profile, "ActionJump" ), "285" ) );
    assert( game_settings_save( settings ) );
    game_settings_close( settings );
    previous_writes = writes;
    assert( game_settings_prepare( directory, "D:\\Games\\SPIDER-MAN WEB OF SHADOWS.EXE", 0x014c ) );
    assert( writes == previous_writes );
    settings = game_settings_open( directory, profile );
    assert( settings && game_settings_get( settings, 0, text, sizeof(text) ) && !strcmp( text, "1920x1080" ) );
    assert( config_get( settings, NULL, "VideoDesiredW", text, sizeof(text) ) && !strcmp( text, "0" ) );
    assert( config_get( settings, NULL, "VideoDesiredH", text, sizeof(text) ) && !strcmp( text, "0" ) );
    assert( game_settings_get( settings, profile_option_index( profile, "MouseSens" ), text, sizeof(text) ) && !strcmp( text, "37.5" ) );
    assert( game_settings_get( settings, profile_option_index( profile, "ActionJump" ), text, sizeof(text) ) && !strcmp( text, "285" ) );
    assert( game_settings_get( settings, profile_option_index( profile, "VideoShadows" ), text, sizeof(text) ) && !strcmp( text, "0" ) );
    assert( strstr( settings->text, "<s id='Mod'>A&amp;B</s>" ) );
    assert( !game_settings_set( settings, profile_option_index( profile, "ActionJump" ), "1.5" ) );
    game_settings_close( settings );
    write_text( path, "<r><s id='VideoW'>800</s>" );
    assert( !game_settings_open( directory, profile ) );
    write_text( path, "<r><s id='VideoW'/><s id='Autorun.SettingsApplied'>1</s></r>" );
    settings = game_settings_open( directory, profile );
    assert( settings && config_get( settings, NULL, "VideoW", text, sizeof(text) ) && !*text );
    assert( game_settings_set( settings, 0, "1280x720" ) && game_settings_save( settings ) );
    game_settings_close( settings );
    assert( !unlink( path ) );
    assert( !mkdir( path, 0777 ) );
    assert( !game_settings_prepare( directory, "Spider-Man Web of Shadows.exe", 0x014c ) );
    assert( !rmdir( path ) );
    memset( oversized, 'x', sizeof(oversized) - 1 );
    oversized[sizeof(oversized) - 1] = 0;
    assert( !game_settings_prepare( oversized, "Spider-Man Web of Shadows.exe", 0x014c ) );
    while (strcmp( path, directory ))
    {
        char *slash = strrchr( path, '/' );
        assert( slash ); *slash = 0;
        assert( !rmdir( path ) );
    }
}

static void check_fallout3_settings(void)
{
    char directory[] = "/tmp/fallout3-settings-XXXXXX", path[1024], value[64];
    const struct game_settings_profile *profile = &fallout3_profile;
    struct game_settings *settings;
    unsigned int resolution = profile_option_index(profile, "iSize W");
    int previous_writes;

    assert(mkdtemp(directory));
    assert(game_settings_find("ums0:/Fallout 3/Fallout3.exe", 0x14c) == profile);
    assert(!game_settings_find("FalloutLauncherSteam.exe", 0x14c));
    assert(!game_settings_find("Fallout3.exe", 0x8664));
    assert(profile->count + 1 <= GAME_SETTINGS_MAX_ROWS);
    assert(game_settings_prepare(directory, "Fallout3.exe", 0x14c));
    settings = game_settings_open(directory, profile);
    assert(settings && game_settings_get(settings, resolution, value, sizeof(value)));
    assert(!strcmp(value, "1280x720") && game_settings_current_preset(settings) == 0);
    for (unsigned int p = 0; p < profile->preset_count; p++)
    {
        assert(game_settings_apply_preset(settings, p));
        assert(game_settings_current_preset(settings) == (int)p);
        for (unsigned int i = 0; i < profile->count; i++)
        {
            const struct game_setting *option = profile->settings + i;
            assert(game_settings_get(settings, i, value, sizeof(value)));
            if (option->presets[p]) assert(strtod(value, NULL) == strtod(option->presets[p], NULL));
        }
    }
    assert(game_settings_set(settings, resolution, "1600x900"));
    assert(game_settings_save(settings));
    snprintf(path, sizeof(path), "%s", settings->path);
    game_settings_close(settings);
    previous_writes = writes;
    assert(game_settings_prepare(directory, "Fallout3.exe", 0x14c) && writes == previous_writes);
    settings = game_settings_open(directory, profile);
    assert(game_settings_get(settings, resolution, value, sizeof(value)) && !strcmp(value, "1600x900"));
    assert(game_settings_current_preset(settings) == 3);
    game_settings_close(settings);
    assert(!unlink(path));
    while (strcmp(path, directory))
    {
        char *slash = strrchr(path, '/');
        assert(slash); *slash = 0;
        assert(!rmdir(path));
    }
}

int main(void)
{
    char directory[] = "/tmp/game-settings-XXXXXX", path[1024], backup[1100];
    struct game_settings *settings, *other;
    int previous_writes;

    check_spiderman_wos_settings();
    check_fallout3_settings();
    assert( mkdtemp( directory ) );
    assert( game_settings_find( "ums0:/games/Fallout4.exe", 0x8664 ) == &fallout4_profile );
    assert( game_settings_find( "C:\\Games\\FALLOUT4.EXE", 0x8664 ) == &fallout4_profile );
    assert( !game_settings_find( "Fallout4Launcher.exe", 0x8664 ) );
    assert( !game_settings_find( "Fallout4.exe", 0x014c ) );
    assert( game_settings_prepare( "/nonexistent", "Another.exe", 0x8664 ) );
    assert( fallout4_profile.count + 1 <= GAME_SETTINGS_MAX_ROWS );

    settings = game_settings_open( directory, &fallout4_profile );
    assert( settings );
    assert( game_settings_current_preset( settings ) == 0 );
    expect( settings, "iSize W", "1280x720" );
    expect( settings, "iTexMipMapSkip", "2" );
    expect( settings, "bNVFlexEnable", "0" );
    expect( settings, "bSAOEnable", "1" );
    assert( game_settings_save( settings ) );
    snprintf( path, sizeof(path), "%s", settings->path );
    snprintf( backup, sizeof(backup), "%s.autorun-old", path );
    game_settings_close( settings );

    /* Existing high-quality settings are initialized once, unrelated keys survive. */
    write_text( path, "\xef\xbb\xbf; [Display] is a comment\r\n[display]\r\n iSize W = 1920\r\n"
                "iSize H=1080\r\nCustomMod=keep\r\n[DISPLAY]\r\niSize W=2560\r\n"
                "[Controls]\r\nSensitivity=0.75\r\n[AudioMenu]\r\nfAudioMasterVolume=0.5\r\n"
                "[SaveGame]\r\nfAutosaveEveryXMins=5\r\n[Mods]\r\nText=[ImageSpace]\r\n" );
    settings = game_settings_open( directory, &fallout4_profile );
    assert( settings && game_settings_save( settings ) );
    expect( settings, "iSize W", "1280x720" );
    assert( !memcmp( settings->text, "\xef\xbb\xbf", 3 ) );
    assert( strstr( settings->text, "CustomMod=keep\r\n" ) );
    assert( strstr( settings->text, "[Controls]\r\nSensitivity=0.75\r\n" ) );
    assert( strstr( settings->text, "fAudioMasterVolume=0.5\r\n" ) );
    assert( strstr( settings->text, "fAutosaveEveryXMins=5\r\n" ) );
    assert( game_settings_set( settings, option_index( "iSize W" ), "960x540" ) );
    assert( game_settings_save( settings ) );
    previous_writes = writes;
    game_settings_close( settings );
    assert( game_settings_prepare( directory, "Fallout4.exe", 0x8664 ) );
    assert( writes == previous_writes );
    settings = game_settings_open( directory, &fallout4_profile );
    expect( settings, "iSize W", "960x540" );
    assert( game_settings_current_preset( settings ) == 0 );
    for (int preset = 0; preset < 4; preset++)
    {
        assert( game_settings_apply_preset( settings, preset ) );
        assert( game_settings_current_preset( settings ) == preset );
        expect( settings, "iSize W", "960x540" );
    }
    assert( game_settings_set( settings, option_index( "sAntialiasing" ), "" ) );
    assert( game_settings_current_preset( settings ) == -1 );
    expect( settings, "sAntialiasing", "" );
    assert( !game_settings_set( settings, option_index( "fShadowDistance" ), "NaN" ) );
    assert( !game_settings_set( settings, option_index( "fShadowDistance" ), "-1" ) );
    assert( !game_settings_set( settings, option_index( "iMaxDecalsPerFrame" ), "1.5" ) );
    assert( !game_settings_set( settings, option_index( "iSize W" ), "1x1" ) );
    assert( !game_settings_set( settings, option_index( "sAntialiasing" ), "TAA\nInjected=1" ) );

    /* Failure to flush or install must not replace the last saved settings. */
    assert( game_settings_set( settings, option_index( "iSize W" ), "1920x1080" ) );
    fail_sync = 1;
    assert( !game_settings_save( settings ) );
    fail_sync = 0;
    other = game_settings_open( directory, &fallout4_profile );
    expect( other, "iSize W", "960x540" );
    game_settings_close( other );
    fail_install = 1;
    assert( !game_settings_save( settings ) );
    fail_install = 0;
    other = game_settings_open( directory, &fallout4_profile );
    expect( other, "iSize W", "960x540" );
    game_settings_close( other );
    game_settings_close( settings );

    /* Recover an interrupted rename, but never replace an existing primary file. */
    assert( !rename( path, backup ) );
    settings = game_settings_open( directory, &fallout4_profile );
    assert( settings && !access( path, F_OK ) && access( backup, F_OK ) );
    expect( settings, "iSize W", "960x540" );
    game_settings_close( settings );

    /* A later external edit stays untouched, including its original formatting. */
    write_text( path, "[Autorun]\nSettingsApplied=1\n[Display]\niSize W=1024\niSize H=576\n" );
    settings = game_settings_open( directory, &fallout4_profile );
    expect( settings, "iSize W", "1024x576" );
    previous_writes = writes;
    assert( game_settings_save( settings ) && writes == previous_writes );
    game_settings_close( settings );

    /* Refuse unsupported encodings and oversized files without truncating them. */
    write_text( path, "\xff\xfe[" );
    assert( !game_settings_open( directory, &fallout4_profile ) );
    FILE *file = fopen( path, "wb" );
    assert( file );
    for (unsigned int i = 0; i <= INI_LIMIT; i++) fputc( 'x', file );
    assert( !fclose( file ) && !game_settings_open( directory, &fallout4_profile ) );
    struct stat st;
    assert( !stat( path, &st ) && st.st_size == INI_LIMIT + 1 );

    /* A second game's definition uses the same persistence implementation. */
    {
        static const struct game_setting options[] = {
            { "Video", "Quality", NULL, "Quality", { "1", "3" }, "2", NULL, 0, GAME_SETTING_INTEGER }
        };
        static const char *const presets[] = { "Low", "High" };
        static const struct game_settings_profile profile = {
            "Another.exe", 0x014c, "other.ini", presets, 2, options, 1,
            GAME_SETTINGS_INI, NULL, { NULL }
        };
        char value[32], other_path[1024];
        other = game_settings_open( directory, &profile );
        assert( other && game_settings_get( other, 0, value, sizeof(value) ) && !strcmp( value, "2" ) );
        assert( game_settings_current_preset( other ) == -1 );
        assert( game_settings_apply_preset( other, 1 ) && game_settings_save( other ) );
        assert( game_settings_current_preset( other ) == 1 );
        assert( game_settings_get( other, 0, value, sizeof(value) ) && !strcmp( value, "3" ) );
        assert( !game_settings_get( other, 1, value, sizeof(value) ) );
        snprintf( other_path, sizeof(other_path), "%s", other->path );
        game_settings_close( other );
        assert( !unlink( other_path ) );
    }
    assert( !unlink( path ) );
    while (strcmp( path, directory ))
    {
        char *slash = strrchr( path, '/' );
        assert( slash ); *slash = 0;
        assert( !rmdir( path ) );
    }
    puts( "Game settings: one-time presets, edit preservation, FAT saves, failure recovery and validation passed" );
    return 0;
}
