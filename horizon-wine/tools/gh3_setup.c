/* Guitar Hero III setup for Wine-NX: the registry key Aspyr's installer writes,
 * written from inside the card.
 *
 * GH3.exe reads Software\Aspyr\Guitar Hero III under HKEY_LOCAL_MACHINE when
 * it starts, and without it says "There was a problem finding or creating one
 * of the game's standard registry entries" and stops. The installer writes two
 * values there:
 *
 * - Path, the game's folder with a trailing backslash. This program lives in
 *   that folder, so its own is the one written, wherever the card puts it.
 * - Language, "en", or what language.txt beside this program says (en, fr, it,
 *   de, es, ko), the ones AWL.dll lists as the game's languages.
 *
 * It also sets the game's screen size to the Switch's, 1280x720, as its
 * options screen would. AWL keeps the game's settings in AspyrConfig.xml under
 * the local application data folder, as <r><s id="Video.Width">...</s></r>, and
 * without one the game asks for 1024x768 in full screen: a mode taller than the
 * 720-line screen, which Wine does not offer, so Direct3D cannot change to it
 * and the game quits. Video.Width and Video.Height are written into the file
 * the game already has, in the encoding it has, keeping everything else in it,
 * or into a new one. The game's own detail options are added too when the file
 * does not have them -- no crowd, flares, physics or front-row camera, and the
 * lower graphics quality -- since the crowd alone is skinned on the CPU every
 * frame; a choice made in the game's Options menu is kept.
 *
 * - dxvk.conf beside the game, where DXVK reads it, unless one is there: with
 *   d3d9.textureMemory = 0, DXVK keeps the game's textures in ordinary memory
 *   rather than in file mappings it can unmap, which on a 32-bit program it
 *   does by default. Every such texture is a mapped view, and on the Switch the
 *   views ran out of room when a song loaded and the game read the NULL
 *   texture it was given.
 *
 * Each step is reported to autorun_runtime.log as a [GH3 SETUP] line; the exit
 * code is 0 when every step worked. Running it again is harmless. */
#include <windows.h>
#include <winternl.h>
#include <shlobj.h>

__declspec(dllimport) NTSTATUS NTAPI NtDisplayString( const UNICODE_STRING *str );

/* Built without a C runtime; the compiler still expects these for structures. */
void *memset( void *dst, int c, size_t n )
{
    unsigned char *p = dst;
    while (n--) *p++ = (unsigned char)c;
    return dst;
}

static unsigned int wide_length( const WCHAR *text )
{
    unsigned int n = 0;
    while (text[n]) n++;
    return n;
}

/* One line: "[GH3 SETUP] <label> <name>: <result>", with the value in hex. */
static void report( const char *label, const WCHAR *name, const char *result, DWORD value )
{
    static const char hex[] = "0123456789abcdef";
    const char *prefix = "[GH3 SETUP] ";
    WCHAR buffer[400];
    UNICODE_STRING str;
    unsigned int n = 0, i;

    while (*prefix) buffer[n++] = *prefix++;
    while (*label && n < 100) buffer[n++] = *label++;
    if (name)
    {
        buffer[n++] = ' ';
        while (*name && n < 320) buffer[n++] = *name++;
    }
    buffer[n++] = ':';
    buffer[n++] = ' ';
    while (*result && n < 380) buffer[n++] = *result++;
    buffer[n++] = ' ';
    buffer[n++] = '0';
    buffer[n++] = 'x';
    for (i = 0; i < 8; i++) buffer[n++] = hex[(value >> (28 - i * 4)) & 15];
    str.Buffer = buffer;
    str.Length = (USHORT)(n * sizeof(WCHAR));
    str.MaximumLength = str.Length;
    NtDisplayString( &str );
}

static BOOL set_string( HKEY key, const WCHAR *name, const WCHAR *value )
{
    LONG status = RegSetValueExW( key, name, 0, REG_SZ, (const BYTE *)value,
                                  (wide_length( value ) + 1) * sizeof(WCHAR) );

    report( "set", name, status ? "failed, error" : "ok", (DWORD)status );
    report( "  value", value, "length", wide_length( value ) );
    return !status;
}

/* The game's folder, with the trailing backslash the installer writes: the
 * one Autorun names when it runs this before the game (runtime.c,
 * run_game_setup_first), else this program's own. */
static BOOL own_folder( WCHAR *out, unsigned int max )
{
    unsigned int n = GetEnvironmentVariableW( L"WINE_NX_GAME_FOLDER", out, max );

    if (n && n + 1 < max)
    {
        if (out[n - 1] != '\\') out[n++] = '\\';
        out[n] = 0;
        return TRUE;
    }
    n = GetModuleFileNameW( NULL, out, max );

    if (!n || n >= max) return FALSE;
    while (n && out[n - 1] != '\\') n--;
    if (!n) return FALSE;
    out[n] = 0;
    return TRUE;
}

/* language.txt beside this program: two letters, anything after them ignored. */
static void chosen_language( const WCHAR *folder, WCHAR *language )
{
    static const char *const known[] = { "en", "fr", "it", "de", "es", "ko" };
    WCHAR path[MAX_PATH];
    char text[8] = { 0 };
    unsigned int n = 0, i;
    DWORD got = 0;
    HANDLE file;

    while (folder[n] && n + 13 < MAX_PATH) { path[n] = folder[n]; n++; }
    for (i = 0; "language.txt"[i]; i++) path[n++] = "language.txt"[i];
    path[n] = 0;
    file = CreateFileW( path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL );
    if (file != INVALID_HANDLE_VALUE)
    {
        ReadFile( file, text, sizeof(text) - 1, &got, NULL );
        CloseHandle( file );
    }
    for (i = 0; i < 2; i++) if (text[i] >= 'A' && text[i] <= 'Z') text[i] += 'a' - 'A';
    for (i = 0; i < sizeof(known) / sizeof(known[0]); i++)
        if (got >= 2 && text[0] == known[i][0] && text[1] == known[i][1]) break;
    if (i == sizeof(known) / sizeof(known[0])) i = 0;
    language[0] = known[i][0];
    language[1] = known[i][1];
    language[2] = 0;
}

/* The config as text, one WCHAR a character: a file with a UTF-16 byte order
 * mark is read as UTF-16, anything else a byte a character, which leaves UTF-8
 * as it was when it is written back, since only ASCII is edited. */
#define CONFIG_MAX (256 * 1024)

static WCHAR *config;
static unsigned int config_len;
static BOOL config_wide;

static unsigned int find( const WCHAR *text, unsigned int from )
{
    unsigned int i, n = wide_length( text );

    for (i = from; i + n <= config_len; i++)
    {
        unsigned int k = 0;
        while (k < n && config[i + k] == text[k]) k++;
        if (k == n) return i;
    }
    return ~0u;
}

static BOOL splice( unsigned int at, unsigned int remove, const WCHAR *text )
{
    unsigned int n = wide_length( text ), i;

    if (config_len - remove + n > CONFIG_MAX) return FALSE;
    if (n > remove) for (i = config_len; i-- > at + remove;) config[i + n - remove] = config[i];
    else for (i = at + remove; i < config_len; i++) config[i + n - remove] = config[i];
    for (i = 0; i < n; i++) config[at + i] = text[i];
    config_len = config_len - remove + n;
    return TRUE;
}

static BOOL set_setting( const WCHAR *name, const WCHAR *value );

/* A setting added only when the file does not have it. */
static BOOL default_setting( const WCHAR *name, const WCHAR *value )
{
    WCHAR text[128];
    unsigned int n = 0, i;

    for (i = 0; L"id=\""[i]; i++) text[n++] = L"id=\""[i];
    for (i = 0; name[i]; i++) text[n++] = name[i];
    text[n++] = '"';
    text[n] = 0;
    if (find( text, 0 ) != ~0u)
    {
        report( "config", name, "kept, the player's; error", 0 );
        return TRUE;
    }
    return set_setting( name, value );
}

/* <s id="name">value</s>: the value replaced where the setting is, or the
 * setting added before </r>. */
static BOOL set_setting( const WCHAR *name, const WCHAR *value )
{
    WCHAR text[128];
    unsigned int n = 0, at, open, close;

    for (at = 0; L"id=\""[at]; at++) text[n++] = L"id=\""[at];
    for (at = 0; name[at]; at++) text[n++] = name[at];
    text[n++] = '"';
    text[n] = 0;
    if ((at = find( text, 0 )) != ~0u && (open = find( L">", at )) != ~0u && (close = find( L"<", open )) != ~0u)
    {
        report( "config", name, "replaced, old length", close - open - 1 );
        return splice( open + 1, close - open - 1, value );
    }
    if ((at = find( L"</r>", 0 )) == ~0u) return FALSE;
    n = 0;
    for (open = 0; L"<s id=\""[open]; open++) text[n++] = L"<s id=\""[open];
    for (open = 0; name[open]; open++) text[n++] = name[open];
    text[n++] = '"';
    text[n++] = '>';
    for (open = 0; value[open]; open++) text[n++] = value[open];
    for (open = 0; L"</s>\r\n"[open]; open++) text[n++] = L"</s>\r\n"[open];
    text[n] = 0;
    report( "config", name, "added, error", 0 );
    return splice( at, 0, text );
}

static BOOL set_screen_size( void )
{
    static const WCHAR fresh[] = L"<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n<r>\r\n</r>\r\n";
    WCHAR path[MAX_PATH + 64];
    unsigned char *bytes;
    unsigned int i, n;
    DWORD got = 0, put = 0;
    HANDLE file;
    HRESULT hr;
    BOOL ok;

    if (FAILED(hr = SHGetFolderPathW( NULL, CSIDL_LOCAL_APPDATA | CSIDL_FLAG_CREATE, NULL, 0, path )))
    {
        report( "config", NULL, "FAILED: no local application data folder, error", (DWORD)hr );
        return FALSE;
    }
    n = wide_length( path );
    for (i = 0; L"\\Aspyr\\Guitar Hero III"[i]; i++) path[n++] = L"\\Aspyr\\Guitar Hero III"[i];
    path[n] = 0;
    SHCreateDirectoryExW( NULL, path, NULL );
    for (i = 0; L"\\AspyrConfig.xml"[i]; i++) path[n++] = L"\\AspyrConfig.xml"[i];
    path[n] = 0;

    config = HeapAlloc( GetProcessHeap(), 0, CONFIG_MAX * sizeof(WCHAR) );
    bytes = HeapAlloc( GetProcessHeap(), 0, CONFIG_MAX * sizeof(WCHAR) );
    if (!config || !bytes) return FALSE;
    file = CreateFileW( path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL );
    if (file != INVALID_HANDLE_VALUE)
    {
        ReadFile( file, bytes, CONFIG_MAX * sizeof(WCHAR), &got, NULL );
        CloseHandle( file );
    }
    config_wide = got >= 2 && bytes[0] == 0xff && bytes[1] == 0xfe;
    if (config_wide)
    {
        for (i = 2, config_len = 0; i + 1 < got; i += 2) config[config_len++] = bytes[i] | (bytes[i + 1] << 8);
    }
    else for (config_len = 0; config_len < got; config_len++) config[config_len] = bytes[config_len];
    if (find( L"</r>", 0 ) == ~0u)
    {
        report( "config", path, got ? "has no <r> element, written anew; bytes" : "not there yet, written; bytes", got );
        config_wide = FALSE;
        for (config_len = 0; fresh[config_len]; config_len++) config[config_len] = fresh[config_len];
    }
    else report( "config", path, config_wide ? "read, UTF-16; bytes" : "read; bytes", got );

    ok = set_setting( L"Video.Width", L"1280" ) && set_setting( L"Video.Height", L"720" ) &&
         default_setting( L"Options.GraphicsQuality", L"1" ) && default_setting( L"Options.Crowd", L"0" ) &&
         default_setting( L"Options.Physics", L"0" ) && default_setting( L"Options.Flares", L"0" ) &&
         default_setting( L"Options.FrontRowCamera", L"0" );
    if (!ok)
    {
        report( "config", path, "FAILED to edit, error", 0 );
        return FALSE;
    }
    n = 0;
    if (config_wide)
    {
        bytes[n++] = 0xff;
        bytes[n++] = 0xfe;
        for (i = 0; i < config_len; i++) { bytes[n++] = config[i] & 0xff; bytes[n++] = config[i] >> 8; }
    }
    else for (i = 0; i < config_len; i++) bytes[n++] = (unsigned char)config[i];
    file = CreateFileW( path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
    if (file == INVALID_HANDLE_VALUE || !WriteFile( file, bytes, n, &put, NULL ) || put != n)
    {
        report( "config", path, "FAILED to write, error", GetLastError() );
        if (file != INVALID_HANDLE_VALUE) CloseHandle( file );
        return FALSE;
    }
    CloseHandle( file );
    report( "config", path, "written, 1280x720; bytes", n );
    return TRUE;
}

static BOOL write_dxvk_conf( const WCHAR *folder )
{
    static const char text[] =
        "# Written by gh3-setup.exe. Guitar Hero III on the Switch: textures in\r\n"
        "# ordinary memory, not in the file mappings DXVK unmaps on 32-bit.\r\n"
        "d3d9.textureMemory = 0\r\n";
    WCHAR path[MAX_PATH];
    unsigned int n = 0, i;
    DWORD put = 0;
    HANDLE file;

    while (folder[n] && n + 10 < MAX_PATH) { path[n] = folder[n]; n++; }
    for (i = 0; "dxvk.conf"[i]; i++) path[n++] = "dxvk.conf"[i];
    path[n] = 0;
    file = CreateFileW( path, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL );
    if (file == INVALID_HANDLE_VALUE)
    {
        DWORD error = GetLastError();

        if (error == ERROR_FILE_EXISTS)
        {
            report( "keep", path, "already there, the player's; error", 0 );
            return TRUE;
        }
        report( "write", path, "FAILED, error", error );
        return FALSE;
    }
    if (!WriteFile( file, text, sizeof(text) - 1, &put, NULL ) || put != sizeof(text) - 1)
    {
        report( "write", path, "FAILED, error", GetLastError() );
        CloseHandle( file );
        return FALSE;
    }
    CloseHandle( file );
    report( "write", path, "ok, d3d9.textureMemory = 0; error", 0 );
    return TRUE;
}

void __stdcall start( void )
{
    WCHAR folder[MAX_PATH], language[3];
    BOOL ok = TRUE;
    HKEY key;
    LONG status;

    if (!own_folder( folder, MAX_PATH ))
    {
        report( "done", NULL, "FAILED: this program's folder is unknown, error", GetLastError() );
        ExitProcess( 1 );
    }
    chosen_language( folder, language );
    if ((status = RegCreateKeyExW( HKEY_LOCAL_MACHINE, L"Software\\Aspyr\\Guitar Hero III", 0, NULL, 0,
                                   KEY_SET_VALUE, NULL, &key, NULL )))
    {
        report( "done", L"Software\\Aspyr\\Guitar Hero III", "FAILED to create, error", (DWORD)status );
        ExitProcess( 1 );
    }
    ok &= set_string( key, L"Path", folder );
    ok &= set_string( key, L"Language", language );
    RegCloseKey( key );
    ok &= set_screen_size();
    ok &= write_dxvk_conf( folder );
    report( ok ? "done, all steps worked" : "done, a step FAILED (see above)", NULL, "error", 0 );
    ExitProcess( !ok );
}
