#!/usr/bin/env python3
"""The AMD64 package brings the runtime and nothing of Windows: its modules are
the DLL repository's. And the Autorun package merges it over the x86 one."""
import ast
import json
from pathlib import Path
from pathlib import PurePosixPath
import re
import shutil
import subprocess
import sys
import tempfile
from zipfile import ZipFile


root = Path(__file__).resolve().parents[2]
package = (root / 'horizon-wine/tools/package-amd64.py').read_text()
for gone in ("'drive_c/windows/system32'", "'drive_c/windows/syswow64'", "'make'", 'dxvk64', 'vkd3d64',
             'winenxaudio', 'make-classes-reg'):
    assert gone not in package, f'package-amd64.py still stages Windows modules ({gone})'

with tempfile.TemporaryDirectory(prefix='autorun-amd64-setup-') as temp:
    temp = Path(temp)
    build = temp / 'build'
    (build / 'licenses').mkdir(parents=True)
    (build / 'CMakeCache.txt').write_text('WINE_NX_AMD64:BOOL=ON\n')
    (build / 'switch-dev.json').write_text('{}\n')
    (build / 'wine-nx-runtime.nro').write_bytes(bytes(16) + b'NRO0')
    (build / 'licenses/FFmpeg-LGPL-2.1.txt').write_text('fixture\n')
    setup = b'MZ setup fixture'
    (build / 'autorun-setup.exe').write_bytes(setup)
    subprocess.run([sys.executable, str(root / 'horizon-wine/tools/package-amd64.py'),
                    '--build', str(build), '--stage-output', str(temp / 'stage')], check=True)
    assert (temp / 'stage/switch/wine/drive_c/windows/autorun-setup.exe').read_bytes() == setup

print('PASS: the AMD64 package includes the Windows component setup')

autorun = root / 'horizon-wine/tools/package-autorun.py'
autorun_tree = ast.parse(autorun.read_text(), filename=str(autorun))
merge = next(node for node in autorun_tree.body
             if isinstance(node, ast.FunctionDef) and node.name == 'merge_amd64')
helpers = ast.Module(body=[merge], type_ignores=[])
ast.fix_missing_locations(helpers)
autorun_namespace = {
    'PurePosixPath': PurePosixPath,
    'ZipFile': ZipFile,
    'json': json,
    're': re,
    'shutil': shutil,
}
exec(compile(helpers, str(autorun), 'exec'), autorun_namespace)

with tempfile.TemporaryDirectory(prefix='autorun-amd64-merge-') as temp:
    temp = Path(temp)
    stage = temp / 'stage'
    runtime = stage / 'switch/wine'
    runtime.mkdir(parents=True)
    for name in ('run-entry.txt', 'target.txt', 'vulkan-probe.txt'):
        (runtime / name).write_text('keep\n')
    archive = temp / 'amd64.zip'
    runtime_markers = re.findall(r'#define WINE_NX_RUNTIME_BUILD "(nx-amd64-[^"]+)"',
                                (root / 'horizon-wine/source/runtime.c').read_text())
    def make_archive(fex, runtime_fex=None):
        manifest = {'features': {name: True for name in
                    ('amd64', 'dynarec', 'vulkan', 'dxvk', 'vkd3d', 'lsfg')}}
        manifest['features']['fex'] = fex
        kind = 'fex-' if (fex if runtime_fex is None else runtime_fex) else 'box64-'
        marker = next(value for value in runtime_markers if value.startswith('nx-amd64-' + kind))
        with ZipFile(archive, 'w') as z:
            z.writestr('switch/wine/build-manifest.json', json.dumps(manifest))
            z.writestr('switch/wine/wine-nx-runtime.nro', f'NRO0 {marker}\0'.encode())
            for name in ('run-entry.txt', 'target.txt', 'vulkan-probe.txt'):
                z.writestr(f'switch/wine/{name}', 'replace\n')
        return marker

    for fex in (False, True):
        expected = make_archive(fex).removeprefix('nx-amd64-')
        if not fex:
            expected = expected.removeprefix('box64-')
        assert autorun_namespace['merge_amd64'](archive, stage) == expected
        for name in ('run-entry.txt', 'target.txt', 'vulkan-probe.txt'):
            assert (runtime / name).read_text() == 'keep\n'

    for fex in (False, True):
        make_archive(fex, runtime_fex=not fex)
        try:
            autorun_namespace['merge_amd64'](archive, stage)
        except AssertionError as error:
            assert 'inconsistent FEX support' in str(error), error
        else:
            raise AssertionError('accepted a runtime/manifest mismatch')

print('PASS: the Autorun package merges the AMD64 runtime without replacing package settings')
