#ifndef AUTORUN_TOMB_RAIDER_H
#define AUTORUN_TOMB_RAIDER_H

#include <stdint.h>
#include "arguments.h"

static inline int tomb_raider_append_arguments( const char *target, uint16_t machine,
                                              char *command_line, size_t size )
{
    static const char argument[] = " -nolauncher";
    const char *name = target;
    size_t length;

    for (const char *p = target; *p; p++) if (*p == '/' || *p == '\\') name = p + 1;
    if (machine != 0x8664 || strcasecmp( name, "SOTTR.exe" )) return 1;
    length = strnlen( command_line, size );
    if (length == size) return 0;
    if (game_has_argument( command_line, "-nolauncher" )) return 1;
    if (sizeof(argument) > size - length) return 0;
    memcpy( command_line + length, argument, sizeof(argument) );
    return 1;
}

#endif
