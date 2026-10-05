#ifndef AUTORUN_LEFT4DEAD_H
#define AUTORUN_LEFT4DEAD_H

#include <stdint.h>
#include "arguments.h"

static inline int left4dead_target( const char *target, uint16_t machine )
{
    const char *name = target;

    for (const char *p = target; *p; p++) if (*p == '/' || *p == '\\') name = p + 1;
    return machine == 0x14c && !strcasecmp( name, "left4dead.exe" );
}

static inline int left4dead_append_arguments( char *command_line, size_t size )
{
    static const char argument[] = " -steam";
    size_t length = strnlen( command_line, size );

    if (length == size) return 0;
    if (game_has_argument( command_line, "-steam" )) return 1;
    if (sizeof(argument) > size - length) return 0;
    memcpy( command_line + length, argument, sizeof(argument) );
    return 1;
}

#define LEFT4DEAD_ENVIRONMENT "SteamAppId=500\0SteamGameId=500\0"

#endif
