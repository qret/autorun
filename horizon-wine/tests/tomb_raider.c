#include <assert.h>
#include <stdio.h>
#include "../source/games/tomb_raider.h"

int main(void)
{
    char command[256] = "\"D:\\Wine\\Shadow of the Tomb Raider\\SOTTR.exe\" -benchmark";
    char previous[256];

    assert(tomb_raider_append_arguments("ums0:/Wine/SOTTR.exe", 0x8664, command, sizeof(command)));
    assert(strstr(command, "-benchmark -nolauncher"));
    strcpy(previous, command);
    assert(tomb_raider_append_arguments("D:\\Wine\\sottr.EXE", 0x8664, command, sizeof(command)));
    assert(!strcmp(command, previous));
    strcpy(command, "game \"-NoLauncher\"");
    strcpy(previous, command);
    assert(tomb_raider_append_arguments("SOTTR.exe", 0x8664, command, sizeof(command)));
    assert(!strcmp(command, previous));
    strcpy(command, "game -nolauncherSuffix");
    assert(tomb_raider_append_arguments("SOTTR.exe", 0x8664, command, sizeof(command)));
    assert(strstr(command, "Suffix -nolauncher"));
    strcpy(command, "game");
    assert(tomb_raider_append_arguments("Other.exe", 0x8664, command, sizeof(command)));
    assert(!strcmp(command, "game"));
    assert(tomb_raider_append_arguments("SOTTR.exe", 0x14c, command, sizeof(command)));
    assert(!strcmp(command, "game"));
    assert(!tomb_raider_append_arguments("SOTTR.exe", 0x8664, command, sizeof("game -nolauncher") - 1));
    assert(!strcmp(command, "game"));
    assert(tomb_raider_append_arguments("SOTTR.exe", 0x8664, command, sizeof("game -nolauncher")));
    assert(!strcmp(command, "game -nolauncher"));
    puts("Tomb Raider launch arguments passed");
    return 0;
}
