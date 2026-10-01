#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "horizon_dlls.h"

int main(int argc, char **argv)
{
    struct horizon_dll_manifest manifest;
    struct horizon_dlls_plan plan;
    unsigned int count = 0;

    assert(argc == 2);
    assert(horizon_dlls_load(argv[1], NULL, 0, &manifest) == HORIZON_DLLS_OK);
    for (unsigned int i = 0; i < manifest.count; i++)
    {
        const struct horizon_dll_file *file = &manifest.files[i];
        if (strcmp(horizon_dlls_folders[file->folder], "drive_c/physx/Engine/v2.7.2")) continue;
        assert(!strcmp(file->name, "physxcore.dll") || !strcmp(file->name, "physxcooking.dll"));
        manifest.files[count++] = *file;
    }
    assert(count == 2);
    manifest.count = count;
    assert(horizon_dlls_plan(argv[1], &manifest, NULL, 1, NULL, NULL, &plan) == HORIZON_DLLS_OK);
    assert(plan.current == 2 && !plan.pending && !plan.unsupported);
    horizon_dlls_free(&manifest);
    puts("PhysX 2.7.2: launcher manifest, folder and payload verification passed");
    return 0;
}
