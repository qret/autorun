#define main existing_game_settings_main
#include "game_settings.c"
#undef main

int main( void )
{
    char directory[] = "/tmp/ff7-settings-XXXXXX", value[64], command[256], original[256];
    const struct game_settings_profile *profile = &ff7remake_profile;
    struct game_settings *settings;
    unsigned int api = profile_option_index( profile, "GraphicsAPI" );
    unsigned int scale = profile_option_index( profile, "r.ScreenPercentage" );
    unsigned int aa = profile_option_index( profile, "r.PostProcessAAQuality" );
    char path[1024];
    int before;

    assert( mkdtemp( directory ) );
    assert( game_settings_find( "D:\\Game\\ff7remake_.exe", 0x8664 ) == profile );
    assert( !game_settings_find( "ff7remake.exe", 0x8664 ) );
    assert( !game_settings_find( "ff7remake_.exe", 0x014c ) );
    assert( game_settings_d3d_api( directory, profile->executable, profile->machine ) == 0 );
    settings = game_settings_open( directory, profile );
    assert( settings && game_settings_save( settings ) );
    for (unsigned int i = 0; i < profile->count; i++)
        assert( !game_settings_get( settings, i, value, sizeof(value) ) );
    strcpy( path, settings->path );
    game_settings_close( settings );

    write_text( path, "; keep this\n[SystemSettings]\nr.ScreenPercentage=75\nr.ScreenPercentage=67\n"
                      "r.UnknownOption=42\n[AnotherSection]\nSomeKey=unchanged\n" );
    settings = game_settings_open( directory, profile );
    assert( settings );
    assert( game_settings_set( settings, scale, "50" ) );
    assert( game_settings_set( settings, aa, "0" ) );
    assert( game_settings_set( settings, api, "11" ) );
    assert( !game_settings_set( settings, api, "10" ) );
    assert( !game_settings_set( settings, scale, "nan" ) );
    assert( !game_settings_set( settings, aa, "0\nOther=1" ) );
    assert( ini_get( settings, "SystemSettings", "r.DynamicRes.MinScreenPercentage", value, sizeof(value) ) && !strcmp(value, "50") );
    assert( ini_get( settings, "SystemSettings", "r.DynamicRes.MaxScreenPercentage", value, sizeof(value) ) && !strcmp(value, "50") );
    assert( strstr( settings->text, "; keep this\n" ) && strstr( settings->text, "r.UnknownOption=42\n" ) &&
            strstr( settings->text, "SomeKey=unchanged\n" ) );
    assert( !strstr( settings->text, "r.ScreenPercentage=67" ) && !strstr( settings->text, "r.ScreenPercentage=75" ) );
    assert( game_settings_save( settings ) );
    game_settings_close( settings );
    assert( game_settings_d3d_api( directory, profile->executable, profile->machine ) == 11 );
    strcpy( command, "\"C:\\Game\\ff7remake_.exe\" -dx12 -D3D12 \"-dx11\" -other=\"-dx12\"" );
    assert( game_settings_append_arguments( directory, profile->executable, profile->machine, command, sizeof(command) ) );
    assert( !strcmp( command, "\"C:\\Game\\ff7remake_.exe\" -other=\"-dx12\" -dx11" ) );
    assert( game_settings_append_arguments( directory, profile->executable, profile->machine, command, sizeof(command) ) );
    assert( !strcmp( command, "\"C:\\Game\\ff7remake_.exe\" -other=\"-dx12\" -dx11" ) );

    strcpy( command, "game.exe" );
    strcpy( original, command );
    assert( !game_set_d3d_argument( command, strlen(command) + 6, 11 ) && !strcmp(command, original) );
    assert( game_set_d3d_argument( command, strlen(command) + 7, 12 ) && !strcmp(command, "game.exe -dx12") );
    strcpy( command, "game.exe -dx11" );
    assert( game_set_d3d_argument( command, strlen(command) + 1, 12 ) && !strcmp(command, "game.exe -dx12") );
    memset( command, 'x', sizeof(command) );
    memcpy( original, command, sizeof(command) );
    assert( !game_set_d3d_argument( command, sizeof(command), 11 ) && !memcmp(command, original, sizeof(command)) );

    settings = game_settings_open( directory, profile );
    before = writes;
    assert( settings && game_settings_save( settings ) && writes == before );
    assert( game_settings_reset( settings, scale ) && game_settings_reset( settings, aa ) );
    assert( !game_settings_get( settings, scale, value, sizeof(value) ) );
    assert( !ini_get( settings, "SystemSettings", "r.DynamicRes.MinScreenPercentage", value, sizeof(value) ) );
    assert( !ini_get( settings, "SystemSettings", "r.DynamicRes.MaxScreenPercentage", value, sizeof(value) ) );
    assert( game_settings_set( settings, api, "12" ) && game_settings_save( settings ) );
    game_settings_close( settings );
    assert( game_settings_d3d_api( directory, profile->executable, profile->machine ) == 12 );
    settings = game_settings_open( directory, profile );
    assert( settings && game_settings_reset( settings, api ) && game_settings_save( settings ) );
    game_settings_close( settings );
    assert( game_settings_d3d_api( directory, profile->executable, profile->machine ) == 0 );
    strcpy( command, "game.exe -dx11" );
    assert( game_settings_append_arguments( directory, profile->executable, profile->machine, command, sizeof(command) ) );
    assert( !strcmp(command, "game.exe -dx11") );

    assert( !unlink( path ) );
    while (strcmp( path, directory ))
    {
        char *slash = strrchr( path, '/' );
        assert( slash ); *slash = 0;
        assert( !rmdir( path ) );
    }
    puts( "FF7 settings: defaults, coupled render scale, reset, preservation and API arguments passed" );
    return 0;
}
