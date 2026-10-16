# Box64 v0.4.4 execution core only. No ELF loader, Linux syscalls, or wrappers.
#
#   wine_nx_add_box64_core(<target> [DYNAREC])
#
# The default is the interpreter. DYNAREC adds Box64's ARM64 dynamic
# recompiler; code is written through one mapping and executed through
# another (source/wow64_box64_dynarec.c), as Horizon requires.

# Replace one exact, reviewed fragment of pinned Box64 source; fail the
# configure step if the pinned text moved.
function(wine_nx_box64_patch content_var anchor replacement what)
    string(FIND "${${content_var}}" "${anchor}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "Pinned Box64 source changed: ${what}")
    endif()
    string(REPLACE "${anchor}" "${replacement}" patched "${${content_var}}")
    set(${content_var} "${patched}" PARENT_SCOPE)
endfunction()

# Write a generated source only when it changed, so a configure run does not
# rebuild every copy.
function(wine_nx_box64_write path content)
    if(EXISTS "${path}")
        file(READ "${path}" old)
        if(old STREQUAL content)
            return()
        endif()
    endif()
    file(WRITE "${path}" "${content}")
endfunction()

# The alias window (source/low_window.c): a fixed-low guest address g below
# 128 MiB lives at wine_nx_box64_window_base + g in a 39-bit process. Code is
# fetched through this; the base is 0 while no window is in use. A function,
# so that a fetch like addr++ is evaluated once.
set(WINE_NX_BOX64_WINDOW_HOST
    "#ifndef WINE_NX_WINDOW_HOST\n#include <stdint.h>\nextern uintptr_t wine_nx_box64_window_base;\nstatic inline uintptr_t wine_nx_window_host(uintptr_t a) { return a < 0x8000000 ? a + wine_nx_box64_window_base : a; }\n#define WINE_NX_WINDOW_HOST(a) wine_nx_window_host((uintptr_t)(a))\n#endif\n")

function(wine_nx_add_box64_core target)
    cmake_parse_arguments(core "DYNAREC" "" "" ${ARGN})
    set(root "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../vendor/box64")
    execute_process(COMMAND git -C "${root}" rev-parse HEAD
        OUTPUT_VARIABLE revision OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE result)
    if(NOT result EQUAL 0 OR NOT revision STREQUAL "2f130fab1d6e1a4ee8a71dc60cfdfcc839ad192a")
        message(FATAL_ERROR "Run horizon-wine/tools/bootstrap-box64-core.sh first")
    endif()
    execute_process(COMMAND git -C "${root}" status --porcelain
        OUTPUT_VARIABLE dirty OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE result)
    if(NOT result EQUAL 0 OR NOT dirty STREQUAL "")
        message(FATAL_ERROR "Box64 vendor checkout must be clean; refusing unreviewed source changes")
    endif()
    set(sources
        x64run0f.c x64run66.c x64run660f.c x64run66f20f.c x64run66f30f.c
        x64run66d9.c x64run66dd.c x64run66f0.c x64rund8.c x64rund9.c
        x64runda.c x64rundb.c x64rundc.c x64rundd.c x64runde.c x64rundf.c
        x64runf0.c x64runf20f.c x64runf30f.c x64runavx.c x64runavx0f.c
        x64runavx0f38.c x64runavx660f.c x64runavxf20f.c x64runavxf30f.c
        x64runavx660f38.c x64runavx660f3a.c x64runavxf20f38.c
        x64runavxf30f38.c x64runavxf20f3a.c x64runavxf30f3a.c
        x64run_private.c x64primop.c x87emu_private.c x64compstrings.c
        x64shaext.c x64emu.c)
    # Keep the vendored revision untouched. The interpreter is built from copies
    # of its sources next to a modrm.h whose fetch macros reach code in the
    # alias window; their quoted includes find that copy first.
    set(window_dir "${CMAKE_CURRENT_BINARY_DIR}/${target}-window")
    file(READ "${root}/src/emu/modrm.h" modrm_header)
    foreach(fetch "(addr++)" "(addr+=2, addr-2)" "(addr+=4, addr-4)" "(addr+=8, addr-8)" "(addr+a)")
        string(REPLACE "*)${fetch}" "*)WINE_NX_WINDOW_HOST(${fetch})" modrm_header "${modrm_header}")
    endforeach()
    string(FIND "${modrm_header}" "WINE_NX_WINDOW_HOST((addr++))" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "Pinned Box64 source changed: modrm.h fetch macros")
    endif()
    wine_nx_box64_write("${window_dir}/emu/modrm.h" "${WINE_NX_BOX64_WINDOW_HOST}${modrm_header}")
    set(emu_sources ${sources})
    set(sources)
    foreach(name ${emu_sources})
        file(READ "${root}/src/emu/${name}" text)
        if(name STREQUAL "x64run_private.c")
            # Operand bytes are fetched here too, and a memory operand's address
            # reaches the window; an LEA's address is a number and does not.
            string(REGEX REPLACE "= \\*\\(([a-z0-9_]+)\\*\\)\\*addr;" "= *(\\1*)WINE_NX_WINDOW_HOST(*addr);" text "${text}")
            string(REPLACE "*)((uint8_t*)(ip+a)))" "*)WINE_NX_WINDOW_HOST((uint8_t*)(ip+a)))" text "${text}")
            string(REPLACE "(*(uint8_t*)(ip+a))" "(*(uint8_t*)WINE_NX_WINDOW_HOST(ip+a))" text "${text}")
            string(REPLACE "(*(int8_t*)(ip+a))" "(*(int8_t*)WINE_NX_WINDOW_HOST(ip+a))" text "${text}")
            wine_nx_box64_patch(text "    if(rex.is32bits)\n        return GetECommon_32(emu, addr, m, rex.offset);"
                "    if(rex.is32bits)\n        return (reg64_t*)WINE_NX_WINDOW_HOST(GetECommon_32(emu, addr, m, rex.offset));"
                "window address of a 32-bit operand")
            wine_nx_box64_patch(text "         return (uintptr_t)&emu->regs[(m&0x07)+(rex.b<<3)];\n    } else return (uintptr_t)GetECommon(emu, addr, rex, m, delta);\n}\n\nreg64_t* GetEw16"
                "         return (uintptr_t)&emu->regs[(m&0x07)+(rex.b<<3)];\n    } else if(rex.is32bits) return (uintptr_t)GetECommon_32(emu, addr, m, rex.offset);\n    else return (uintptr_t)GetECommon(emu, addr, rex, m, delta);\n}\n\nreg64_t* GetEw16"
                "LEA keeps the guest address")
            string(PREPEND text "${WINE_NX_BOX64_WINDOW_HOST}")
        endif()
        wine_nx_box64_write("${window_dir}/emu/${name}" "${text}")
        list(APPEND sources "${window_dir}/emu/${name}")
    endforeach()
    list(APPEND sources "${root}/src/tools/bitutils.c")

    # The only other interpreter change is a before-fetch hook; assert its
    # insertion point against the pinned source.
    file(READ "${root}/src/emu/x64run.c" run_source)
    set(anchor "    while(1) \n#endif\n    {")
    wine_nx_box64_patch(run_source "${anchor}"
        "${anchor}\n        if (wine_nx_box64_before_instruction(emu, addr)) return 0;"
        "interpreter hook anchor")
    wine_nx_box64_patch(run_source "#include \"modrm.h\""
        "#include \"modrm.h\"\nextern int wine_nx_box64_before_instruction(x64emu_t *, uintptr_t);"
        "interpreter hook include")
    set(generated "${window_dir}/emu/x64run.c")
    wine_nx_box64_write("${generated}" "${run_source}")

    file(READ "${root}/src/emu/x64emu_private.h" emu_header)
    wine_nx_box64_patch(emu_header
        "    #ifdef _WIN32\n    uint64_t    win64_teb;\n    #endif"
        "    #if defined(_WIN32) || defined(WINE_NX_BOX64_X18_TLS)\n    uint64_t    win64_teb;\n    #endif"
        "host TLS storage")
    string(PREPEND emu_header "#include <stdint.h>\n")
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Generic")
        string(PREPEND emu_header "#ifndef _GNU_SOURCE\n#define _GNU_SOURCE\n#endif\n")
    endif()
    set(emu_generated "${CMAKE_CURRENT_BINARY_DIR}/${target}-x64emu_private.h")
    file(WRITE "${emu_generated}" "${emu_header}")

    # Settings shared by the core and the per-pass dynarec objects.
    add_library(${target}-settings INTERFACE)
    target_include_directories(${target}-settings SYSTEM INTERFACE "${root}/src/include" "${root}/src"
        "${root}/src/emu" "${root}/src/wrapped/generated")
    target_compile_definitions(${target}-settings INTERFACE ARM64 CONFIG_64BIT STATICBUILD
        WINE_NX_BOX64_X18_TLS)
    target_compile_options(${target}-settings INTERFACE -ffixed-x18)
    if(CMAKE_SYSTEM_NAME STREQUAL "Generic")
        target_include_directories(${target}-settings SYSTEM INTERFACE
            "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../switch-shims"
            "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../box64-shims")
    endif()
    set(private_options -O1 -ffunction-sections -fdata-sections -Wno-unused-result)
    # Track core atomic locks so a guest memory fault can unwind without
    # leaving the native mutex locked. The adapter itself uses real pthreads.
    set(private_definitions
        pthread_mutex_lock=wine_nx_box64_mutex_lock
        pthread_mutex_unlock=wine_nx_box64_mutex_unlock)

    if(core_DYNAREC AND CMAKE_SYSTEM_NAME STREQUAL "Generic")
        file(READ "${root}/src/include/os.h" os_source)
        wine_nx_box64_patch(os_source "#define LongJmp longjmp"
            "#define LongJmp(a, b) longjmp((a)->state, b)" "Horizon longjmp")
        wine_nx_box64_patch(os_source "#define SigSetJmp sigsetjmp"
            "struct wine_nx_jump_buffer { jmp_buf state; };\n#define SigSetJmp(a, b) setjmp((a)->state)" "Horizon setjmp")
        wine_nx_box64_patch(os_source "#define JUMPBUFF struct __jmp_buf_tag"
            "#define JUMPBUFF struct wine_nx_jump_buffer" "Horizon jump buffer")
        set(os_generated "${CMAKE_CURRENT_BINARY_DIR}/${target}-os.h")
        file(WRITE "${os_generated}" "${os_source}")
    endif()

    add_library(${target} STATIC ${sources} "${generated}")
    target_link_libraries(${target} PUBLIC ${target}-settings)
    target_compile_definitions(${target} PRIVATE ${private_definitions})
    target_compile_options(${target} PRIVATE ${private_options})
    if(os_generated)
        target_compile_options(${target} PRIVATE
            "$<$<COMPILE_LANGUAGE:C>:SHELL:-include ${os_generated}>"
            "$<$<COMPILE_LANGUAGE:C>:SHELL:-include ${emu_generated}>")
    else()
        target_compile_options(${target} PRIVATE
            "$<$<COMPILE_LANGUAGE:C>:SHELL:-include ${emu_generated}>")
    endif()
    set_source_files_properties(
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../source/wow64_box64_engine.c"
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../source/wow64_box64_dynarec.c"
        PROPERTIES COMPILE_OPTIONS "-include;${emu_generated}")

    if(NOT core_DYNAREC)
        return()
    endif()

    # ARM64 dynarec: the same file set Box64's CMakeLists.txt uses for
    # ARM_DYNAREC, without the x64test harness.
    set(dynarec_sources dynarec.c dynablock.c dynarec_native_functions.c dynacache_reloc.c)
    list(TRANSFORM dynarec_sources PREPEND "${root}/src/dynarec/")
    # geted_ib, fakeed and their neighbours fetch code with macros of their own.
    list(REMOVE_ITEM dynarec_sources "${root}/src/dynarec/dynarec_native_functions.c")
    file(READ "${root}/src/dynarec/dynarec_native_functions.c" functions_source)
    wine_nx_box64_patch(functions_source "#define F8      *(uint8_t*)(addr++)"
        "#define F8      *(uint8_t*)WINE_NX_WINDOW_HOST(addr++)" "geted_ib code fetch")
    wine_nx_box64_patch(functions_source "(((uint8_t*)ip)[A])"
        "(((uint8_t*)WINE_NX_WINDOW_HOST(ip))[A])" "native function code peek")
    wine_nx_box64_write("${window_dir}/dynarec/dynarec_native_functions.c"
        "${WINE_NX_BOX64_WINDOW_HOST}${functions_source}")
    list(APPEND dynarec_sources "${window_dir}/dynarec/dynarec_native_functions.c")
    # Count hash validations of translated blocks, reported by the runtime.
    list(REMOVE_ITEM dynarec_sources "${root}/src/dynarec/dynablock.c")
    file(READ "${root}/src/dynarec/dynablock.c" dynablock_source)
    wine_nx_box64_patch(dynablock_source
        "        //if (db->always_test) SchedYield(); // just calm down...\n        uint32_t hash = X31_hash_code((void*)db->x64_readaddr, db->x64_size);"
        "        //if (db->always_test) SchedYield(); // just calm down...\n        extern unsigned int wine_nx_box64_block_tests;\n        __atomic_add_fetch(&wine_nx_box64_block_tests, 1, __ATOMIC_RELAXED);\n        uint32_t hash = X31_hash_code((void*)db->x64_readaddr, db->x64_size);"
        "count block validations")
    # CALLRET marks a block's return sites ARCH_UDF when the block may have
    # changed and ARCH_NOP once it is checked, in place: through the writable
    # alias, since block pointers are executable-alias addresses. Marking a
    # block dirty also flushes the caches, as every other rewrite does; a
    # stale fetch would run the NOP and return into changed code unchecked.
    string(PREPEND dynablock_source "void* DynarecMapWritableAddress(void* addr);\n")
    # Translation is not free: the passes that decide how large a block will be
    # run before it asks the arenas for room. Once the last code memory object
    # is full every entry into untranslated code translated it again, threw the
    # work away and interpreted it -- the main thread of The Sims 2 spent all
    # of its time in the translator. A block that already exists is still
    # returned; only new ones are refused, and those are interpreted.
    wine_nx_box64_patch(dynablock_source
        "        return block;\n    }\n\n    #ifndef WIN32"
        "        return block;\n    }\n    {\n        extern int wine_nx_box64_code_room(void);\n        if(!wine_nx_box64_code_room())\n            return NULL;\n    }\n\n    #ifndef WIN32"
        "no translation without room for the block")
    # Every system call and unix call ends at a gate, which is on a page the
    # dynarec may not translate. Box64 finds that out only after taking the
    # global translator lock, twice per gate (LinkNext, then EmuRun), so every
    # thread calling out contended for it. The protection check is lock-free
    # here; Box64 repeats it under the lock for pages that are executable.
    wine_nx_box64_patch(dynablock_source
        "    pthread_sigmask(SIG_BLOCK, &critical_prot, &old_sig);\n    if(need_lock) {"
        "    if((getProtection_fast(addr)&req_prot)!=req_prot)\n        return NULL;\n    pthread_sigmask(SIG_BLOCK, &critical_prot, &old_sig);\n    if(need_lock) {"
        "refuse untranslatable pages before the translator lock")
    wine_nx_box64_patch(dynablock_source "*(uint32_t*)(db->block+db->callrets[i].offs)"
        "*(uint32_t*)DynarecMapWritableAddress(db->block+db->callrets[i].offs)" "callret site writes")
    wine_nx_box64_patch(dynablock_source "uint32_t hash = X31_hash_code((void*)db->x64_readaddr, db->x64_size);"
        "uint32_t hash = X31_hash_code((void*)WINE_NX_WINDOW_HOST(db->x64_readaddr), db->x64_size);"
        "hash a block's code through the alias window")
    string(PREPEND dynablock_source "${WINE_NX_BOX64_WINDOW_HOST}")
    set(dynablock_generated "${CMAKE_CURRENT_BINARY_DIR}/${target}-dynablock.c")
    file(WRITE "${dynablock_generated}" "${dynablock_source}")
    list(APPEND dynarec_sources "${dynablock_generated}")
    if(CMAKE_SYSTEM_NAME STREQUAL "Generic")
        set_source_files_properties("${dynablock_generated}" PROPERTIES
            COMPILE_DEFINITIONS "pthread_sigmask=wine_nx_box64_sigmask")
    endif()
    list(REMOVE_ITEM dynarec_sources "${root}/src/dynarec/dynarec.c")
    file(READ "${root}/src/dynarec/dynarec.c" dispatch_source)
    # A gate or the run's completion address ends the run before the block
    # lookup: otherwise it fails, and the interpreter starts only for its hook
    # to stop the run again and unwind with longjmp.
    wine_nx_box64_patch(dispatch_source "            dynablock_t* block = (skip || ACCESS_FLAG(F_TF))?NULL:fastDBGetBlock(emu, R_RIP, 1, is32bits);"
        "            extern int wine_nx_box64_stop_at(x64emu_t* emu, uintptr_t pc);\n            if(wine_nx_box64_stop_at(emu, R_RIP))\n                break;\n            dynablock_t* block = (skip || ACCESS_FLAG(F_TF))?NULL:fastDBGetBlock(emu, R_RIP, 1, is32bits);"
        "end runs at gates without a block lookup")
    wine_nx_box64_patch(dispatch_source
        "void* LinkNext(x64emu_t* emu, uintptr_t addr, void* x2, uintptr_t* x3)\n{\n    int is32bits = (R_CS == 0x23);"
        "void* LinkNext(x64emu_t* emu, uintptr_t addr, void* x2, uintptr_t* x3)\n{\n    extern int wine_nx_box64_link_stop_at(x64emu_t*, uintptr_t);\n    if(wine_nx_box64_link_stop_at(emu, addr)) return native_epilog;\n    int is32bits = (R_CS == 0x23);"
        "end AMD64 runs before linking native targets")
    wine_nx_box64_patch(dispatch_source "                } else\n                    native_prolog(emu, jblock);"
        "                } else {\n                    extern unsigned long long wine_nx_box64_native_entries;\n                    __atomic_add_fetch(&wine_nx_box64_native_entries, 1, __ATOMIC_RELAXED);\n                    native_prolog(emu, jblock);\n                }" "count native dispatch entries")
    set(dispatch_generated "${CMAKE_CURRENT_BINARY_DIR}/${target}-dynarec.c")
    file(WRITE "${dispatch_generated}" "${dispatch_source}")
    list(APPEND dynarec_sources "${dispatch_generated}")
    set(arm64_sources dynarec_arm64_functions.c dynarec_arm64_arch.c arm64_immenc.c
        arm64_printer.c dynarec_arm64_jmpnext.c dynarec_arm64_consts.c)
    list(TRANSFORM arm64_sources PREPEND "${root}/src/dynarec/arm64/")
    list(REMOVE_ITEM arm64_sources "${root}/src/dynarec/arm64/dynarec_arm64_functions.c")
    file(READ "${root}/src/dynarec/arm64/dynarec_arm64_functions.c" arm64_functions_source)
    wine_nx_box64_patch(arm64_functions_source "#define F8      *(uint8_t*)(addr++)\n#define F32S64  (uint64_t)(int64_t)*(int32_t*)(addr+=4, addr-4)"
        "#define F8      *(uint8_t*)WINE_NX_WINDOW_HOST(addr++)\n#define F32S64  (uint64_t)(int64_t)*(int32_t*)WINE_NX_WINDOW_HOST((addr+=4, addr-4))"
        "arm64 function code fetch")
    wine_nx_box64_write("${window_dir}/dynarec/arm64/dynarec_arm64_functions.c"
        "${WINE_NX_BOX64_WINDOW_HOST}${arm64_functions_source}")
    list(APPEND arm64_sources "${window_dir}/dynarec/arm64/dynarec_arm64_functions.c")
    if(CMAKE_SYSTEM_NAME STREQUAL "Generic")
        # Linux signal-frame reconstruction is not a Horizon implementation.
        list(REMOVE_ITEM arm64_sources "${root}/src/dynarec/arm64/dynarec_arm64_arch.c")
        file(READ "${root}/src/dynarec/arm64/dynarec_arm64_arch.c" arch_source)
        wine_nx_box64_patch(arch_source "#ifndef _WIN32 // TODO: Implemented this for Win32"
            "#if 0 /* Horizon uses its own exception boundary. */" "Horizon excludes Linux adjust_arch")
        set(arch_generated "${CMAKE_CURRENT_BINARY_DIR}/${target}-arch.c")
        file(WRITE "${arch_generated}" "${arch_source}")
        list(APPEND arm64_sources "${arch_generated}")
    endif()
    set(asm_sources arm64_prolog.S arm64_epilog.S arm64_next.S arm64_lock.S)
    list(TRANSFORM asm_sources PREPEND "${root}/src/dynarec/arm64/")
    set(pass_sources dynarec_arm64_helper.c dynarec_arm64_emit_tests.c
        dynarec_arm64_emit_math.c dynarec_arm64_emit_logic.c dynarec_arm64_emit_shift.c
        dynarec_arm64_00.c dynarec_arm64_0f.c dynarec_arm64_66.c dynarec_arm64_d8.c
        dynarec_arm64_d9.c dynarec_arm64_da.c dynarec_arm64_db.c dynarec_arm64_dc.c
        dynarec_arm64_dd.c dynarec_arm64_de.c dynarec_arm64_df.c dynarec_arm64_f0.c
        dynarec_arm64_660f.c dynarec_arm64_66f20f.c dynarec_arm64_66f30f.c
        dynarec_arm64_66f0.c dynarec_arm64_f20f.c dynarec_arm64_f30f.c
        dynarec_arm64_avx.c dynarec_arm64_avx_0f.c dynarec_arm64_avx_0f38.c
        dynarec_arm64_avx_66_0f.c dynarec_arm64_avx_f2_0f.c dynarec_arm64_avx_f3_0f.c
        dynarec_arm64_avx_66_0f38.c dynarec_arm64_avx_66_0f3a.c
        dynarec_arm64_avx_f2_0f38.c dynarec_arm64_avx_f2_0f3a.c
        dynarec_arm64_avx_f3_0f38.c updateflags_arm64_pass.c)
    list(TRANSFORM pass_sources PREPEND "${root}/src/dynarec/arm64/")
    list(APPEND pass_sources "${root}/src/dynarec/dynarec_native_pass.c")

    list(REMOVE_ITEM pass_sources "${root}/src/dynarec/dynarec_native_pass.c")
    file(READ "${root}/src/dynarec/dynarec_native_pass.c" pass_source)
    wine_nx_box64_patch(pass_source
        "    while(ok) {\n        #if STEP == 0"
        "    while(ok) {\n        #if STEP == 0\n        extern int wine_nx_box64_translate_allowed(uintptr_t);\n        if(!is32bits && !wine_nx_box64_translate_allowed(addr)) {\n            need_epilog = 1;\n            break;\n        }"
        "stop AMD64 blocks before unsupported vector prefixes")
    wine_nx_box64_patch(pass_source "|| !(*(uint32_t*)addr))) {"
        "|| !(*(uint32_t*)WINE_NX_WINDOW_HOST(addr)))) {" "block end check through the alias window")
    wine_nx_box64_patch(pass_source "            if(*(uint32_t*)addr!=0) {"
        "            if(*(uint32_t*)WINE_NX_WINDOW_HOST(addr)!=0) {" "big block check through the alias window")
    string(PREPEND pass_source "${WINE_NX_BOX64_WINDOW_HOST}")
    set(pass_generated "${CMAKE_CURRENT_BINARY_DIR}/${target}-dynarec_native_pass.c")
    file(WRITE "${pass_generated}" "${pass_source}")
    list(APPEND pass_sources "${pass_generated}")

    list(REMOVE_ITEM pass_sources "${root}/src/dynarec/arm64/dynarec_arm64_helper.c")
    file(READ "${root}/src/dynarec/arm64/dynarec_arm64_helper.c" helper_source)
    wine_nx_box64_patch(helper_source
        "    #ifdef _WIN32\n    LDRx_U12(xR8, xEmu, offsetof(x64emu_t, win64_teb));\n    #endif"
        "    #if defined(_WIN32) || defined(WINE_NX_BOX64_X18_TLS)\n    LDRx_U12(xR8, xEmu, offsetof(x64emu_t, win64_teb));\n    #endif"
        "restore host TLS for helper calls")
    # An operand of 32-bit code below 128 MiB lives in the alias window. The
    # whole address is computed into a register (no displacement left for the
    # load) and then moved by the base: xR15 (x25) holds R15, which 32-bit code has
    # not got, and the flags, which hold the guest's, are not touched. An
    # absolute address is moved when the block is translated. LEA asks for the
    # number itself and sets wine_nx_box64_geted_raw.
    wine_nx_box64_patch(helper_source
        "    *fixaddress = 0;\n    if(hint>0) ret = hint;\n    if(hint>0 && hint<xRAX) scratch = hint;"
        "    *fixaddress = 0;\n    if(hint>0) ret = hint;\n    if(hint>0 && hint<xRAX) scratch = hint;\n    extern unsigned int wine_nx_box64_window_shift;\n    extern int wine_nx_box64_geted_raw;\n    int window = rex.is32bits && !rex.seg && wine_nx_box64_window_shift && !wine_nx_box64_geted_raw;\n    int window_done = 0;\n    if(window) {\n        absmax = 0;\n        mask = 0;\n        unscaled = NULL;\n    }"
        "geted: no displacement left in the alias window")
    wine_nx_box64_patch(helper_source
        "                    } else\n                        MOV64y(ret, tmp);\n"
        "                    } else if(window && (uint32_t)tmp < 0x8000000) {\n                        MOV64x(ret, (uint64_t)(uint32_t)tmp + wine_nx_box64_window_base);\n                        window_done = 1;\n                    } else\n                        MOV64y(ret, tmp);\n"
        "geted: absolute SIB address in the alias window")
    wine_nx_box64_patch(helper_source
        "                } else\n                    MOV32w(ret, tmp);"
        "                } else if(window && (uint32_t)tmp < 0x8000000) {\n                    MOV64x(ret, (uint64_t)(uint32_t)tmp + wine_nx_box64_window_base);\n                    window_done = 1;\n                } else\n                    MOV32w(ret, tmp);"
        "geted: absolute address in the alias window")
    wine_nx_box64_patch(helper_source
        "    *ed = ret;\n    return addr;\n}\n\n/* setup r2 to address pointed by ED"
        "    if(window && !window_done) {\n        uint8_t dst = IS_GPR(ret) ? ((hint>0 && hint<xRAX) ? hint : x2) : ret;\n        LSRw(xR15, ret, 27);\n        SUBx_U12(xR15, xR15, 1);\n        LSRx(xR15, xR15, 63);\n        ADDx_REG_LSL(dst, ret, xR15, wine_nx_box64_window_shift);\n        ret = dst;\n    }\n    *ed = ret;\n    return addr;\n}\n\n/* setup r2 to address pointed by ED"
        "geted: move an operand into the alias window")
    string(PREPEND helper_source "${WINE_NX_BOX64_WINDOW_HOST}")
    set(helper_generated "${CMAKE_CURRENT_BINARY_DIR}/${target}-dynarec_arm64_helper.c")
    file(WRITE "${helper_generated}" "${helper_source}")
    list(APPEND pass_sources "${helper_generated}")
    # CALLRET pushes a native return pair for each CALL and pops it at the RET.
    # Pairs of calls that never return stay until a RET misses or the block
    # exits, which on Linux is a growing 8 MB stack, but a Wine thread here has
    # 1 MB: past 64 KB, drop them as a missed RET does (the prolog's zero pair
    # then makes the older RETs miss). x3 is scratch at both CALLs.
    list(REMOVE_ITEM pass_sources "${root}/src/dynarec/arm64/dynarec_arm64_00.c")
    file(READ "${root}/src/dynarec/arm64/dynarec_arm64_00.c" opcodes_source)
    set(callret_depth_guard
        "                        ADDx_U12(x3, xSP, 0);\n                        SUBx_REG(x3, xSavedSP, x3);\n                        LSRx(x3, x3, 16);\n                        CBZx(x3, 2*4);\n                        SUBx_U12(xSP, xSavedSP, 16);\n")
    wine_nx_box64_patch(opcodes_source "                        STPx_S7_preindex(x4, x2, xSP, -16);"
        "${callret_depth_guard}                        STPx_S7_preindex(x4, x2, xSP, -16);" "bound CALL return pairs")
    wine_nx_box64_patch(opcodes_source "                        STPx_S7_preindex(x4, xRIP, xSP, -16);"
        "${callret_depth_guard}                        STPx_S7_preindex(x4, xRIP, xSP, -16);" "bound CALL Ed return pairs")
    wine_nx_box64_patch(opcodes_source
        "                addr = geted(dyn, addr, ninst, nextop, &ed, gd, &fixedaddress, NULL, 0, 0, rex, NULL, 0, 0);\n                if(gd!=ed) {    // it's sometimes used as a 3 bytes NOP"
        "                wine_nx_box64_geted_raw = 1;\n                addr = geted(dyn, addr, ninst, nextop, &ed, gd, &fixedaddress, NULL, 0, 0, rex, NULL, 0, 0);\n                wine_nx_box64_geted_raw = 0;\n                if(gd!=ed) {    // it's sometimes used as a 3 bytes NOP"
        "LEA keeps the guest address")
    string(PREPEND opcodes_source "extern int wine_nx_box64_geted_raw;\n")
    set(opcodes_generated "${CMAKE_CURRENT_BINARY_DIR}/${target}-dynarec_arm64_00.c")
    file(WRITE "${opcodes_generated}" "${opcodes_source}")
    list(APPEND pass_sources "${opcodes_generated}")

    # Every pass is built from a copy next to a dynarec_arm64_helper.h whose
    # code fetch macros reach the alias window; a copy's quoted includes find
    # it, and the vendored headers for everything else.
    file(READ "${root}/src/dynarec/arm64/dynarec_arm64_helper.h" arm64_helper_header)
    foreach(fetch "(addr++)" "(addr+=2, addr-2)" "(addr+=4, addr-4)" "(addr+=8, addr-8)" "(addr+a)" "(ip+a)")
        string(REPLACE "*)${fetch}" "*)WINE_NX_WINDOW_HOST(${fetch})" arm64_helper_header "${arm64_helper_header}")
    endforeach()
    string(FIND "${arm64_helper_header}" "WINE_NX_WINDOW_HOST((addr++))" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "Pinned Box64 source changed: dynarec code fetch macros")
    endif()
    wine_nx_box64_write("${window_dir}/dynarec/arm64/dynarec_arm64_helper.h"
        "${WINE_NX_BOX64_WINDOW_HOST}${arm64_helper_header}")
    file(READ "${root}/src/dynarec/dynarec_helper.h" dynarec_helper_header)
    wine_nx_box64_write("${window_dir}/dynarec/dynarec_helper.h" "${dynarec_helper_header}")
    set(window_passes)
    foreach(source ${pass_sources})
        get_filename_component(name "${source}" NAME)
        string(REPLACE "${target}-" "" name "${name}")
        file(READ "${source}" text)
        if(name STREQUAL "dynarec_arm64_66.c")
            wine_nx_box64_patch(text
                "                addr = geted(dyn, addr, ninst, nextop, &ed, x3, &fixedaddress, NULL, 0, 0, rex, NULL, 0, 0);\n                BFIx(gd, ed, 0, 16);"
                "                wine_nx_box64_geted_raw = 1;\n                addr = geted(dyn, addr, ninst, nextop, &ed, x3, &fixedaddress, NULL, 0, 0, rex, NULL, 0, 0);\n                wine_nx_box64_geted_raw = 0;\n                BFIx(gd, ed, 0, 16);"
                "16-bit LEA keeps the guest address")
            string(PREPEND text "extern int wine_nx_box64_geted_raw;\n")
        endif()
        if(name STREQUAL "dynarec_native_pass.c")
            set(window_pass "${window_dir}/dynarec/${name}")
        else()
            set(window_pass "${window_dir}/dynarec/arm64/${name}")
        endif()
        wine_nx_box64_write("${window_pass}" "${text}")
        list(APPEND window_passes "${window_pass}")
    endforeach()
    set(pass_sources ${window_passes})

    # Split code mapping. Blocks are emitted through the writable alias
    # AllocDynarecMap returns; once complete, the pointers the dynarec executes
    # and publishes are moved to the executable alias. Box64's emitted code is
    # position independent, so the bytes are valid at either address.
    set(split_map_decl "void* DynarecMapExecutableAddress(void* addr);\nvoid* DynarecMapWritableAddress(void* addr);\nvoid DynarecMapClearCache(void* addr, size_t size);\n")
    set(to_exec
        "    block->actual_block = DynarecMapExecutableAddress(block->actual_block);\n    block->block = DynarecMapExecutableAddress(block->block);\n    block->jmpnext = DynarecMapExecutableAddress(block->jmpnext);\n")

    file(READ "${root}/src/dynarec/dynarec_native.c" native_source)
    # Marking a block's instructions alive recursed once an instruction, and a
    # block of MAX_INSTS wants far more stack than the 1 MB a Wine thread has;
    # FalloutNV died there with 16 bytes of stack left. The same edges are
    # walked from an explicit stack: an index is stacked only as it is marked,
    # so the block's own instruction count bounds it, and FillBlock already
    # holds the translator lock the neighbouring static arrays rely on.
    wine_nx_box64_patch(native_source
        "static void recurse_mark_alive(dynarec_native_t* dyn, int i)\n{\n    if(dyn->insts[i].x64.alive)\n        return;\n    dyn->insts[i].x64.alive = 1;\n    if(dyn->insts[i].x64.jmp && dyn->insts[i].x64.jmp_insts!=-1)\n        recurse_mark_alive(dyn, dyn->insts[i].x64.jmp_insts);\n    if(i<dyn->size-1 && dyn->insts[i].x64.has_next)\n        recurse_mark_alive(dyn, i+1);\n}"
        "static int static_alive[MAX_INSTS+2];\nstatic void recurse_mark_alive(dynarec_native_t* dyn, int i)\n{\n    int top = 0;\n    if(dyn->insts[i].x64.alive)\n        return;\n    dyn->insts[i].x64.alive = 1;\n    static_alive[top++] = i;\n    while(top) {\n        i = static_alive[--top];\n        if(dyn->insts[i].x64.jmp && dyn->insts[i].x64.jmp_insts!=-1) {\n            int jmpto = dyn->insts[i].x64.jmp_insts;\n            if(!dyn->insts[jmpto].x64.alive) {\n                dyn->insts[jmpto].x64.alive = 1;\n                static_alive[top++] = jmpto;\n            }\n        }\n        if(i<dyn->size-1 && dyn->insts[i].x64.has_next && !dyn->insts[i+1].x64.alive) {\n            dyn->insts[i+1].x64.alive = 1;\n            static_alive[top++] = i+1;\n        }\n    }\n}"
        "walk alive marks without recursion")
    wine_nx_box64_patch(native_source
        "    uint32_t prot = getProtection_fast(addr);"
        "    extern int wine_nx_box64_translate_allowed(uintptr_t);\n    if(!is32bits && !wine_nx_box64_translate_allowed(addr)) return NULL;\n    uint32_t prot = getProtection_fast(addr);"
        "refuse AMD64 blocks beginning with unsupported vector prefixes")
    wine_nx_box64_patch(native_source "void ClearCache(void* start, size_t len)\n{\n#if defined(ARM64)"
        "${split_map_decl}void ClearCache(void* start, size_t len)\n{\n    DynarecMapClearCache(start, len);\n#if 0"
        "dynarec_native.c ClearCache")
    wine_nx_box64_patch(native_source
        "    ClearCache(actual_p+sizeof(void*), JMPNEXT_SIZE-sizeof(void*));   // need to clear the cache before execution...\n    return block;\n}"
        "    ClearCache(actual_p+sizeof(void*), JMPNEXT_SIZE-sizeof(void*));   // need to clear the cache before execution...\n${to_exec}    return block;\n}"
        "dynarec_native.c CreateEmptyBlock")
    wine_nx_box64_patch(native_source
        "    redundant_helper = current_helper = NULL;\n    //block->done = 1;\n    return block;\n}"
        "    redundant_helper = current_helper = NULL;\n${to_exec}    //block->done = 1;\n    return block;\n}"
        "dynarec_native.c FillBlock64")
    # Guest code pages stay writable, so translated blocks are not write
    # protected. winebox64 reports freed, unmapped, re-protected and flushed
    # guest memory, and wine_nx_box64_invalidate frees or marks the blocks
    # there; otherwise blocks link directly. The largest block size bounds how
    # far before a range a block may start.
    wine_nx_box64_patch(native_source "            //block->x64_addr = (void*)start;\n            block->x64_size = end-start;"
        "            //block->x64_addr = (void*)start;\n            block->x64_size = end-start;\n            { extern void wine_nx_box64_note_block_size(size_t); wine_nx_box64_note_block_size(block->x64_size); }"
        "record the largest block size")
    wine_nx_box64_patch(native_source "*(uint32_t*)(block->block+block->callrets[i].offs)"
        "*(uint32_t*)DynarecMapWritableAddress(block->block+block->callrets[i].offs)" "always-dirty callret site marks")
    wine_nx_box64_patch(native_source "uint32_t hash = X31_hash_code((void*)addr, end-addr);"
        "uint32_t hash = X31_hash_code((void*)WINE_NX_WINDOW_HOST(addr), end-addr);" "hash code through the alias window")
    wine_nx_box64_patch(native_source "block->hash = X31_hash_code((void*)block->x64_readaddr, block->x64_size);"
        "block->hash = X31_hash_code((void*)WINE_NX_WINDOW_HOST(block->x64_readaddr), block->x64_size);"
        "hash a new block through the alias window")
    string(PREPEND native_source "${WINE_NX_BOX64_WINDOW_HOST}")
    set(native_generated "${CMAKE_CURRENT_BINARY_DIR}/${target}-dynarec_native.c")
    file(WRITE "${native_generated}" "${native_source}")

    file(READ "${root}/src/dynarec/arm64/updateflags_arm64.c" flags_source)
    wine_nx_box64_patch(flags_source "static uint8_t dummy_code[]"
        "${split_map_decl}static uint8_t dummy_code[]" "updateflags_arm64.c declarations")
    wine_nx_box64_patch(flags_source
        "    ClearCache(actual_p+sizeof(void*), native_size);   // need to clear the cache before execution...\n\n    updaflags_arm64 = block;"
        "    ClearCache(actual_p+sizeof(void*), native_size);   // need to clear the cache before execution...\n${to_exec}\n    updaflags_arm64 = block;"
        "updateflags_arm64.c block pointers")
    set(flags_generated "${CMAKE_CURRENT_BINARY_DIR}/${target}-updateflags_arm64.c")
    file(WRITE "${flags_generated}" "${flags_source}")

    list(REMOVE_ITEM asm_sources
        "${root}/src/dynarec/arm64/arm64_epilog.S"
        "${root}/src/dynarec/arm64/arm64_next.S")
    foreach(asm_name arm64_epilog arm64_next)
        file(READ "${root}/src/dynarec/arm64/${asm_name}.S" asm_source)
        wine_nx_box64_patch(asm_source "#ifdef _WIN32"
            "#if defined(_WIN32) || defined(WINE_NX_BOX64_X18_TLS)" "${asm_name} host TLS restore")
        set(asm_generated "${CMAKE_CURRENT_BINARY_DIR}/${target}-${asm_name}.S")
        file(WRITE "${asm_generated}" "${asm_source}")
        list(APPEND asm_sources "${asm_generated}")
    endforeach()

    target_compile_definitions(${target}-settings INTERFACE DYNAREC SAVE_MEM WINE_NX_BOX64_DYNAREC)
    # Generated copies still include their neighbours by relative path.
    set_source_files_properties("${native_generated}" PROPERTIES
        INCLUDE_DIRECTORIES "${root}/src/dynarec")
    set_source_files_properties("${flags_generated}" PROPERTIES
        INCLUDE_DIRECTORIES "${root}/src/dynarec/arm64;${root}/src/dynarec")
    target_sources(${target} PRIVATE ${dynarec_sources} ${arm64_sources} ${asm_sources}
        "${native_generated}" "${flags_generated}")
    target_include_directories(${target} PRIVATE "${root}/src/dynarec" "${root}/src/dynarec/arm64")
    # arm64_lock.S carries an optional LSE path chosen at run time.
    set_source_files_properties(${asm_sources} PROPERTIES
        COMPILE_OPTIONS "-march=armv8.1-a+lse+crc+crypto")

    # The code generator is compiled once per pass (STEP=0..3).
    foreach(step 0 1 2 3)
        add_library(${target}-pass${step} OBJECT ${pass_sources})
        target_link_libraries(${target}-pass${step} PRIVATE ${target}-settings)
        target_compile_definitions(${target}-pass${step} PRIVATE STEP=${step} ${private_definitions})
        target_compile_options(${target}-pass${step} PRIVATE ${private_options})
        if(CMAKE_SYSTEM_NAME STREQUAL "Generic")
            target_compile_options(${target}-pass${step} PRIVATE
                "SHELL:-include ${os_generated}" "SHELL:-include ${emu_generated}")
        else()
            target_compile_options(${target}-pass${step} PRIVATE
                "$<$<COMPILE_LANGUAGE:C>:SHELL:-include ${emu_generated}>")
        endif()
        target_include_directories(${target}-pass${step} PRIVATE "${root}/src/dynarec" "${root}/src/dynarec/arm64")
        target_sources(${target} PRIVATE $<TARGET_OBJECTS:${target}-pass${step}>)
    endforeach()
endfunction()
