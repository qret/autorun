#ifndef AUTORUN_GAME_ARGUMENTS_H
#define AUTORUN_GAME_ARGUMENTS_H

#include <string.h>
#include <strings.h>

static inline int game_has_argument( const char *command_line, const char *key )
{
    size_t length = strlen( key );

    while (*command_line)
    {
        const char *argument;
        int quoted = 0;
        while (*command_line == ' ' || *command_line == '\t') command_line++;
        argument = command_line + (*command_line == '"');
        if (!strncasecmp( argument, key, length ) &&
            (key[length - 1] == '=' || !argument[length] || argument[length] == '"' ||
             argument[length] == ' ' || argument[length] == '\t')) return 1;
        for (; *command_line; command_line++)
        {
            if (*command_line == '"') quoted = !quoted;
            if (!quoted && (*command_line == ' ' || *command_line == '\t')) break;
        }
    }
    return 0;
}

#endif
