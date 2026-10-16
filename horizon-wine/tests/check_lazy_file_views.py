#!/usr/bin/env python3
"""Views of files read on demand (dlls/ntdll/unix/horizon.c): a read-only view
of a real file is a reservation that remembers its file, and a fault reads one
chunk of it. Every piece a view is split into keeps the file and its own
offset, and the file is closed with the last piece."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[2]
source = (root / 'dlls/ntdll/unix/horizon.c').read_text()
runtime = (root / 'horizon-wine/source/runtime.c').read_text()

# Only what nothing can write through, of a real file, and large enough.
wanted = source[source.index('static BOOL horizon_lazy_file_view_wanted'):]
wanted = wanted[:wanted.index('\n}\n')]
for need in ('wine_nx_lazy_file_views', 'fd != -1', 'HORIZON_LAZY_FILE_VIEW_MIN',
             '!(prot & (PROT_WRITE | PROT_EXEC))', '!horizon_memfile_from_fd( fd )'):
    assert need in wanted, need

# Mappings are freed through free_mapping, which lets the file go.
assert 'horizon_object_free( &mapping_pool, mapping );\n}' in source  # inside free_mapping only
assert source.count('horizon_object_free( &mapping_pool,') == 1
assert 'file_source_release( mapping->file );' in source

# Splits keep the file: both sides of a commit, and all three pieces of a protect.
replace = source[source.index('static int replace_reservation_mapping'):]
replace = replace[:replace.index('\nstatic int change_reservation_mapping')]
assert replace.count('inherit_file_source(') == 2
assert 'mapping->file_offset + (start - mapping_start)' in replace
protect = source[source.index('static int protect_reservation_mapping'):]
protect = protect[:protect.index('\nstatic int split_backing_mapping')]
assert protect.count('inherit_file_source(') == 3

# A fault reads a file chunk, smaller than an anonymous one.
fault = source[source.index('static BOOL horizon_commit_lazy_fault'):]
assert 'mapping->file ? HORIZON_LAZY_FILE_VIEW_CHUNK : HORIZON_LAZY_MAPPING_CHUNK' in fault

# Only MAP_FIXED views are made this way, which is how Wine maps files.
fixed = source[source.index('static void *horizon_mmap_fixed'):]
fixed = fixed[:fixed.index('\nvoid *horizon_anon_mmap_fixed')]
assert 'horizon_lazy_file_view_wanted( fd, size, prot ) && !add_lazy_file_mapping( start, size, prot, fd, offset )' in fixed
# A view that cannot be read on demand is read whole: the branch falls through
# to the ordinary one rather than failing the view.
lazy_branch = fixed[fixed.index('horizon_lazy_file_view_wanted( fd, size, prot )'):]
assert lazy_branch.index('stage = "reserve file view read on demand";') < lazy_branch.index('stage = "map backing";')
assert 'MAP_FAILED' not in lazy_branch[:lazy_branch.index('stage = "map backing";')]

# A write's buffer is brought in before the write locks the SD cache or hands
# it to a service: only the parts in views read on demand, found under
# mapping_mutex and touched after it, since the fault takes that lock.
touch = source[source.index('void horizon_touch_buffer('):]
touch = touch[:touch.index('\n}\n')]
assert 'if (!wine_nx_lazy_file_views' in touch
assert 'mapping->file && mapping->reservation' in touch
assert touch.index('pthread_mutex_unlock( &mapping_mutex );') < touch.index('(void)*byte;')
file_c = (root / 'dlls/ntdll/unix/file.c').read_text()
write = file_c[file_c.index('NTSTATUS WINAPI NtWriteFile('):]
assert write.index('horizon_touch_buffer( buffer, length );') < write.index('server_get_unix_fd(')
socket_c = (root / 'dlls/ntdll/unix/socket.c').read_text()
send = socket_c[socket_c.index('static NTSTATUS sock_send('):]
assert send.index('horizon_touch_buffer( async->iov[i].iov_base, async->iov[i].iov_len );') < send.index('SERVER_START_REQ( send_socket )')

# The runtime turns it on per program, and says so.
assert 'wine_nx_lazy_file_views = settings.lazy_file_views;' in runtime
settings = (root / 'horizon-wine/source/launcher_settings.h').read_text()
assert 'settings->lazy_file_views = launcher_setting_state( kv, "lazy-file-views" ) == 1;' in settings
assert 'launcher_kv_set( kv, "lazy-file-views", settings->lazy_file_views ? "1" : NULL )' in settings
# A switch in each program's settings, off unless turned on.
launcher = (root / 'horizon-wine/source/launcher.c').read_text()
assert 'ADD_ROW( ROW_LAZY_FILE_VIEWS, SECTION_EMULATION, "Fast file loading",' in launcher
assert 'case ROW_LAZY_FILE_VIEWS:' in launcher
assert 'lazy-file-views.txt' in runtime
assert '[INIT] views of files read on demand' in runtime
print('lazy file views: ok')
