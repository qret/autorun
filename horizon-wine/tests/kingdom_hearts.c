#include <assert.h>
#include <stdio.h>
#include "../source/games/kingdom_hearts.h"

int main(void)
{
    char args[1024], previous[1024];
    const char *targets[] = {
        "D:/Wine/KINGDOM HEARTS III.exe",
        "D:\\Wine\\KINGDOM HEARTS FINAL MIX.exe",
        "ums0:/Wine/kingdom hearts ii final mix.EXE"
    };

    for (unsigned int i = 0; i < sizeof(targets) / sizeof(*targets); i++)
    {
        strcpy(args, "\"D:\\Wine\\Game.exe\"");
        assert(kingdom_hearts_append_arguments(targets[i], 0x8664, args, sizeof(args)));
        assert(strstr(args, i ? "-epicapp=68c214c58f694ae88c2dab6f209b43e4" :
                               "-epicapp=fd711544a06543e0ab1b0808de334120"));
        assert(strstr(args, i ? "-AUTH_PASSWORD=00000000000000000000000000000000" :
                               "-AUTH_PASSWORD=unused"));
        strcpy(previous, args);
        assert(kingdom_hearts_append_arguments(targets[i], 0x8664, args, sizeof(args)));
        assert(!strcmp(previous, args));

        strcpy(args, "game -AUTH_LOGIN=user -auth_password=token -epicapp=custom -epicuserid=123 \"-EpicPortal\"");
        assert(kingdom_hearts_append_arguments(targets[i], 0x8664, args, sizeof(args)));
        assert(strstr(args, "-auth_password=token") && !strstr(args, "-AUTH_PASSWORD="));
        assert(strstr(args, "-epicapp=custom") && !strstr(args, "-epicapp=68c"));
        assert(!strstr(args, "-epicuserid=1638"));

        strcpy(args, "game");
        assert(!kingdom_hearts_append_arguments(targets[i], 0x8664, args, 8));
        assert(!strcmp(args, "game"));
        assert(kingdom_hearts_append_arguments(targets[i], 0x14c, args, sizeof(args)));
        assert(!strcmp(args, "game"));
    }
    strcpy(args, "game");
    assert(kingdom_hearts_append_arguments("KINGDOM HEARTS HD 1.5+2.5 Launcher.exe", 0x8664, args, sizeof(args)));
    assert(!strcmp(args, "game"));
    assert(!kingdom_hearts_has_argument("-EpicPortalSuffix -other=-epicapp=custom", "-EpicPortal"));
    assert(!kingdom_hearts_has_argument("-other=-epicapp=custom", "-epicapp="));
    puts("Kingdom Hearts arguments: profiles, overrides, capacity and idempotence passed");
    return 0;
}
