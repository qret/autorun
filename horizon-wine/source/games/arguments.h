#ifndef AUTORUN_GAME_ARGUMENTS_H
#define AUTORUN_GAME_ARGUMENTS_H

#include <string.h>
#include <strings.h>
#include <stddef.h>

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

static inline int game_d3d_argument( const char *begin, const char *end )
{
    static const char *const flags[] = { "-dx11", "-d3d11", "-dx12", "-d3d12" };

    if (end > begin && *begin == '"' && end[-1] == '"') { begin++; end--; }
    for (size_t i = 0; i < sizeof(flags) / sizeof(flags[0]); i++)
        if ((size_t)(end - begin) == strlen( flags[i] ) && !strncasecmp( begin, flags[i], end - begin )) return 1;
    return 0;
}

static inline int game_set_d3d_argument( char *command_line, size_t size, int api )
{
    const char *flag = api == 11 ? " -dx11" : " -dx12";
    char *read = command_line, *write = command_line;
    size_t length = strnlen( command_line, size ), removed = 0;

    if (length == size || (api != 11 && api != 12)) return 0;
    for (int pass = 0; pass < 2; pass++)
    {
        read = write = command_line;
        while (*read)
        {
            char *space = read, *begin;
            int quoted = 0;
            while (*read == ' ' || *read == '\t') read++;
            begin = read;
            while (*read)
            {
                if (*read == '"') quoted = !quoted;
                if (!quoted && (*read == ' ' || *read == '\t')) break;
                read++;
            }
            if (game_d3d_argument( begin, read ))
            {
                if (!pass) removed += read - space;
            }
            else if (pass)
            {
                memmove( write, space, read - space );
                write += read - space;
            }
        }
        if (!pass && strlen(flag) >= size - (length - removed)) return 0;
    }
    memcpy( write, flag, strlen(flag) + 1 );
    return 1;
}

#endif
