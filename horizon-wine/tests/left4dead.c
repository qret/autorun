#include <assert.h>
#include <stdio.h>
#include "../source/games/left4dead.h"

int main(void)
{
    char command[256] = "\"D:\\Wine\\Left 4 Dead\\left4dead.exe\" -novid";
    char previous[256];
    static const char environment[] = LEFT4DEAD_ENVIRONMENT;

    assert(left4dead_target("ums0:/Wine/Left 4 Dead/left4dead.exe", 0x14c));
    assert(left4dead_target("D:\\Wine\\LEFT4DEAD.EXE", 0x14c));
    assert(!left4dead_target("left4dead.exe", 0x8664));
    assert(!left4dead_target("left4dead2.exe", 0x14c));
    assert(!left4dead_target("L4D Launcher.exe", 0x14c));
    assert(left4dead_append_arguments(command, sizeof(command)));
    assert(strstr(command, "-novid -steam"));
    strcpy(previous, command);
    assert(left4dead_append_arguments(command, sizeof(command)));
    assert(!strcmp(command, previous));
    strcpy(command, "game \"-Steam\"");
    strcpy(previous, command);
    assert(left4dead_append_arguments(command, sizeof(command)));
    assert(!strcmp(command, previous));
    strcpy(command, "game -steamSuffix");
    assert(left4dead_append_arguments(command, sizeof(command)));
    assert(strstr(command, "Suffix -steam"));
    strcpy(command, "game");
    assert(!left4dead_append_arguments(command, sizeof("game -steam") - 1));
    assert(!strcmp(command, "game"));
    assert(left4dead_append_arguments(command, sizeof("game -steam")));
    assert(!strcmp(command, "game -steam"));
    assert(!strcmp(environment, "SteamAppId=500"));
    assert(!strcmp(environment + strlen(environment) + 1, "SteamGameId=500"));
    assert(environment[sizeof(environment) - 2] == 0 && environment[sizeof(environment) - 1] == 0);
    puts("Left 4 Dead launch arguments and environment passed");
    return 0;
}
