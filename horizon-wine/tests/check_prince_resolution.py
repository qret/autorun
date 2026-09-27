#!/usr/bin/env python3
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / 'horizon-wine/source/runtime.c').read_text(encoding='utf-8')


def extract(signature):
    start = source.index(signature)
    end = source.index('{', start) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end] + '\n'


fixture = r'''
#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <strings.h>
#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winternl.h"
#include "horizon_registry.h"
#define WINE_NX_FB_W 1280
#define WINE_NX_FB_H 720
static struct horizon_reg registry;
static unsigned int handles, writes, calls, fail_create, fail_write, messages;
static long long now(void) { return 1000; }
static void log_line(const char *format, ...) { (void)format; messages++; }
NTSTATUS WINAPI NtCreateKey(HANDLE *key, ACCESS_MASK access, const OBJECT_ATTRIBUTES *attr,
                            ULONG title, const UNICODE_STRING *class_name, ULONG options, ULONG *disposition)
{
    struct horizon_reg_key *created;
    unsigned int status;
    (void)title; (void)class_name; (void)disposition;
    assert(access == (KEY_CREATE_SUB_KEY | KEY_SET_VALUE));
    assert(!attr->RootDirectory && attr->Attributes == OBJ_CASE_INSENSITIVE);
    assert(!options);
    if (++calls == fail_create) return STATUS_ACCESS_DENIED;
    status = horizon_reg_create(&registry, NULL, attr->ObjectName->Buffer,
                               attr->ObjectName->Length, 0, options, NULL, 0, &created);
    if (status && status != HORIZON_REG_NAME_EXISTS) return status;
    *key = created;
    handles++;
    return STATUS_SUCCESS;
}
NTSTATUS WINAPI NtClose(HANDLE key)
{
    assert(handles);
    handles--;
    horizon_reg_release(&registry, key);
    return STATUS_SUCCESS;
}
NTSTATUS WINAPI NtSetValueKey(HANDLE key, const UNICODE_STRING *name, ULONG title,
                              ULONG type, const void *data, ULONG size)
{
    (void)title;
    assert(type == REG_DWORD && size == sizeof(ULONG));
    if (++writes == fail_write) return STATUS_ACCESS_DENIED;
    return horizon_reg_set_value(&registry, key, name->Buffer, name->Length, type, data, size);
}
'''
fixture += extract('static const char *path_basename(')
fixture += extract('static NTSTATUS runtime_create_registry_path(')
fixture += extract('static NTSTATUS runtime_prepare_prince_of_persia(')
fixture += r'''
static void expect(struct horizon_reg_key *key, const char *name, ULONG value)
{
    for (unsigned int i = 0; i < key->value_count; i++)
    {
        struct horizon_reg_value *entry = &key->values[i];
        size_t length = strlen(name), j;
        if (entry->namelen != length * 2) continue;
        for (j = 0; j < length; j++) if (entry->name[j] != name[j]) break;
        if (j != length) continue;
        assert(entry->type == REG_DWORD && entry->len == sizeof(value));
        assert(!memcmp(entry->data, &value, sizeof(value)));
        return;
    }
    assert(!"missing value");
}
int main(void)
{
    static const char *const paths[] = {
        "sdmc:/Roms/PC/Prince of Persia/PrinceOfPersia_Launcher.exe",
        "ums0:/Games/Prince of Persia.exe",
        "C:\\Games\\PRINCEOFPERSIA_LAUNCHER.EXE"
    };
    struct horizon_reg_key *user, *engine;
    const char seed[] = "WINE REGISTRY Version 2\n"
        "[Software\\\\Ubisoft\\\\Prince of Persia\\\\1.0\\\\Engine]\n"
        "\"ScreenResolutionWidth\"=dword:00000320\n"
        "\"ScreenResolutionHeight\"=dword:00000258\n"
        "\"LNG_Language\"=dword:00000002\n"
        "\"VerticalSync\"=dword:00000001\n";
    unsigned int errors = 0;
    assert(horizon_reg_init(&registry, now, NULL));
    assert(!runtime_prepare_prince_of_persia("sdmc:/OtherGame.exe"));
    assert(!runtime_prepare_prince_of_persia("PrinceOfPersia_Launcher.exe.bak"));
    assert(!runtime_prepare_prince_of_persia("Launcher.exe"));
    assert(!calls && !writes && !messages);
    assert(!runtime_prepare_prince_of_persia(paths[0]));
    assert(!handles && writes == 2 && messages == 1);
    user = horizon_reg_create_ascii(&registry, registry.root, "User\\S-1-5-21-0-0-0-1000");
    assert(user);
    assert(!horizon_reg_load(&registry, user, seed, sizeof(seed) - 1, &errors) && !errors);
    engine = horizon_reg_create_ascii(&registry, user, "Software\\Ubisoft\\Prince of Persia\\1.0\\Engine");
    for (unsigned int i = 0; i < ARRAY_SIZE(paths); i++)
    {
        writes = messages = 0;
        assert(!runtime_prepare_prince_of_persia(paths[i]));
        assert(!handles && writes == 2 && messages == 1);
        expect(engine, "ScreenResolutionWidth", 1280);
        expect(engine, "ScreenResolutionHeight", 720);
        expect(engine, "LNG_Language", 2);
        expect(engine, "VerticalSync", 1);
        assert(engine->value_count == 4);
    }
    for (unsigned int i = 1; i <= 2; i++)
    {
        writes = messages = 0;
        fail_write = i;
        assert(runtime_prepare_prince_of_persia(paths[0]) == STATUS_ACCESS_DENIED);
        assert(!handles && !messages && writes == i);
    }
    fail_write = 0;
    writes = 0;
    fail_create = calls + 3;
    assert(runtime_prepare_prince_of_persia(paths[0]) == STATUS_ACCESS_DENIED);
    assert(!handles && !writes && !messages);
    horizon_reg_release(&registry, registry.root);
    puts("Prince of Persia: new/existing registry, SD/USB paths, 720p, unrelated settings and failure cleanup passed");
    return 0;
}
'''

with tempfile.TemporaryDirectory(prefix='prince-resolution-') as directory:
    build = Path(directory)
    path = build / 'test.c'
    path.write_text(fixture)
    subprocess.run([os.environ.get('CC', 'clang'), '-std=gnu11', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', '-D__WINESRC__', '-D_WIN64',
                    '-I' + str(root / 'include'), '-I' + str(root / 'dlls/ntdll/unix'),
                    str(path), '-o', str(build / 'test')], check=True)
    subprocess.run([str(build / 'test')], check=True)
