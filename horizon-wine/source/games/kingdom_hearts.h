#ifndef AUTORUN_KINGDOM_HEARTS_H
#define AUTORUN_KINGDOM_HEARTS_H

#include <stdint.h>
#include "arguments.h"

static inline int kingdom_hearts_append_arguments( const char *target, uint16_t machine,
                                                 char *command_line, size_t size )
{
    const char *name = target, *app, *password;
    size_t length, needed = 0;
    unsigned int missing = 0;

    for (const char *p = target; *p; p++) if (*p == '/' || *p == '\\') name = p + 1;
    if (machine != 0x8664) return 1;
    if (!strcasecmp( name, "KINGDOM HEARTS III.exe" ))
    {
        app = " -epicapp=fd711544a06543e0ab1b0808de334120";
        password = " -AUTH_PASSWORD=unused";
    }
    else if (!strcasecmp( name, "KINGDOM HEARTS FINAL MIX.exe" ) ||
             !strcasecmp( name, "KINGDOM HEARTS II FINAL MIX.exe" ))
    {
        app = " -epicapp=68c214c58f694ae88c2dab6f209b43e4";
        password = " -AUTH_PASSWORD=00000000000000000000000000000000";
    }
    else return 1;

    const struct { const char *key, *value; } arguments[] = {
        { "-AUTH_LOGIN=", " -AUTH_LOGIN=unused" },
        { "-AUTH_PASSWORD=", password },
        { "-AUTH_TYPE=", " -AUTH_TYPE=exchangecode" },
        { "-epicapp=", app },
        { "-epicenv=", " -epicenv=Prod" },
        { "-EpicPortal", " -EpicPortal" },
        { "-epicusername=", " -epicusername=Player" },
        { "-epicuserid=", " -epicuserid=1638" },
        { "-epiclocale=", " -epiclocale=en" }
    };
    length = strnlen( command_line, size );
    if (length == size) return 0;
    for (unsigned int i = 0; i < sizeof(arguments) / sizeof(arguments[0]); i++)
        if (!game_has_argument( command_line, arguments[i].key ))
        {
            missing |= 1u << i;
            needed += strlen( arguments[i].value );
        }
    if (needed >= size - length) return 0;
    for (unsigned int i = 0; i < sizeof(arguments) / sizeof(arguments[0]); i++)
        if (missing & (1u << i))
        {
            size_t count = strlen( arguments[i].value );
            memcpy( command_line + length, arguments[i].value, count + 1 );
            length += count;
        }
    return 1;
}

#endif
