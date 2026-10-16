// The alias window under FEX: a 32-bit guest's memory below 128 MiB lives only
// at window base + address (low_window.c, wine_nx_alias_window_probe), so every
// guest access below 128 MiB has to be moved there by the alias window pass,
// and nothing the guest sees (ESP, ESI, EDI, results) may carry the base.
//   fex_window window    every case runs and checks its results
//   fex_window none      no window: the first guest access must fault at its
//                        low address, which shows the cases do reach the window
#include <FEXCore/Config/Config.h>
#include <FEXCore/Core/Context.h>
#include <FEXCore/Core/HostFeatures.h>
#include <FEXCore/Debug/InternalThreadState.h>
#include <FEXCore/HLE/SyscallHandler.h>
#include <FEXCore/Utils/LogManager.h>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/mman.h>
#include <unistd.h>
#include <vector>

using namespace FEXCore::X86State;
// the cases below name only the registers they set; the rest start at zero
#pragma clang diagnostic ignored "-Wmissing-field-initializers"

static constexpr uintptr_t window = uintptr_t(1) << 32, code = 0x100000, data = 0x110000;
static constexpr uintptr_t high = 0x9000000, edge = 0x8000000;
static bool expect_fault;

class Syscalls final : public FEXCore::HLE::SyscallHandler {
public:
  void HandleSyscall(FEXCore::Core::CpuStateFrame*) override { std::abort(); }
  FEXCore::HLE::ExecutableRangeInfo QueryGuestExecutableRange(FEXCore::Core::InternalThreadState*, uint64_t) override {
    return {code, code + 0x1000, true};
  }
  std::optional<FEXCore::ExecutableFileSectionInfo> LookupExecutableFileSection(FEXCore::Core::InternalThreadState*, uint64_t) override {
    return std::nullopt;
  }
};

static void fault_handler(int, siginfo_t* info, void*) {
  const auto address = reinterpret_cast<uintptr_t>(info->si_addr);
  if (expect_fault && address >= data && address < data + 0x10000) {
    printf("FEX window none: first access faulted at %lx, below 128 MiB, as it must without the window\n",
           static_cast<unsigned long>(address));
    fflush(stdout);
    _exit(0);
  }
  printf("FAIL: fault at %lx\n", static_cast<unsigned long>(address));
  fflush(stdout);
  _exit(3);
}

static void* map(uintptr_t address, size_t size) {
  auto p = mmap(reinterpret_cast<void*>(address), size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
  if (p != reinterpret_cast<void*>(address)) {
    printf("cannot map %lx\n", static_cast<unsigned long>(address));
    _exit(2);
  }
  return p;
}

template<typename T>
static T& at(uintptr_t address) {
  return *reinterpret_cast<T*>(address);
}
// guest address g below 128 MiB, as the host sees it
static uintptr_t host(uintptr_t g) {
  return g < edge ? window + g : g;
}

struct Regs {
  uint32_t eax, ebx, ecx, edx, esi, edi, esp;
};
struct Case {
  const char* name;
  std::vector<uint8_t> bytes;
  Regs in;
  void (*setup)();
  bool (*check)(const Regs&);
};

static constexpr uint32_t sp = data + 0x8ff0;

int main(int argc, char** argv) {
  if (argc != 2 && argc != 3) return 2;
  const int only = argc == 3 ? atoi(argv[2]) : -1;
  setvbuf(stdout, nullptr, _IONBF, 0);
  const bool with_window = !strcmp(argv[1], "window");
  expect_fault = !with_window;
  if (with_window) setenv("WINE_NX_FEX_WINDOW_SHIFT", "32", 1);
  LogMan::Throw::InstallHandler([](const char* message) { fprintf(stderr, "%s\n", message); std::abort(); });

  // with the window the game's code is only in it, as Autorun maps the image
  // there (virtual.c, map_image_view); without it the code sits at its address
  const uintptr_t code_host = with_window ? window + code : code;
  map(code_host, 0x1000);
  memset(reinterpret_cast<void*>(code_host), 0xcc, 0x1000);
  map(window + data, 0x10000);
  map(window + edge - 0x1000, 0x1000);
  map(edge, 0x1000);
  map(high, 0x10000);
  struct sigaction action {};
  action.sa_sigaction = fault_handler;
  action.sa_flags = SA_SIGINFO;
  sigaction(SIGSEGV, &action, nullptr);
  sigaction(SIGBUS, &action, nullptr);

  FEXCore::Config::Initialize();
  FEXCore::Config::Load();
  FEXCore::Config::Set(FEXCore::Config::CONFIG_IS64BIT_MODE, "0");
  FEXCore::Config::Set(FEXCore::Config::CONFIG_TSOENABLED, "1");
  FEXCore::HostFeatures features {};
  features.DCacheLineLog2 = 4;
  features.HostType = FEXCore::HostFeatures::HostTypeEnum::Wow64;
  features.CPUMIDRs.assign(4, 0x411fd071);
  auto owner = FEXCore::Context::Context::CreateNewContext(features);
  auto* context = owner.get();
  Syscalls syscalls;
  FEXCore::SignalDelegator signals;
  context->SetSignalDelegator(&signals);
  context->SetSyscallHandler(&syscalls);
  context->EnableExitOnHLT();
  if (!context->InitCore()) return 2;

  const std::vector<Case> cases = {
    {"moffs load", {0xa1, 0x00, 0x00, 0x11, 0x00}, {}, [] { at<uint32_t>(host(data)) = 0x11223344; },
     [](const Regs& r) { return r.eax == 0x11223344; }},
    {"store base+disp", {0x89, 0x4b, 0x08}, {.ebx = data + 0x100, .ecx = 0xcafef00d}, nullptr,
     [](const Regs& r) { return at<uint32_t>(host(data + 0x108)) == 0xcafef00d && r.ebx == data + 0x100; }},
    {"load base+index*4+disp", {0x8b, 0x44, 0x8b, 0x10}, {.ebx = data + 0x200, .ecx = 3},
     [] { at<uint32_t>(host(data + 0x21c)) = 0x5a5a1234; }, [](const Regs& r) { return r.eax == 0x5a5a1234; }},
    {"push pop", {0x51, 0x5a, 0x53}, {.ebx = 0x77777777, .ecx = 0x12345678, .esp = sp}, nullptr,
     [](const Regs& r) { return r.edx == 0x12345678 && r.esp == sp - 4 && at<uint32_t>(host(sp - 4)) == 0x77777777; }},
    {"pop to memory", {0x8f, 0x03}, {.ebx = data + 0x300, .esp = sp}, [] { at<uint32_t>(host(sp)) = 0xabcdef01; },
     [](const Regs& r) { return at<uint32_t>(host(data + 0x300)) == 0xabcdef01 && r.esp == sp + 4; }},
    {"rep movsd", {0xf3, 0xa5}, {.ecx = 4, .esi = data + 0x400, .edi = data + 0x500},
     [] { for (int i = 0; i < 4; ++i) at<uint32_t>(host(data + 0x400 + 4 * i)) = 0x1000 + i; },
     [](const Regs& r) {
       for (int i = 0; i < 4; ++i)
         if (at<uint32_t>(host(data + 0x500 + 4 * i)) != uint32_t(0x1000 + i)) return false;
       return r.ecx == 0 && r.esi == data + 0x410 && r.edi == data + 0x510;
     }},
    {"movsb", {0xa4}, {.esi = data + 0x600, .edi = data + 0x700}, [] { at<uint8_t>(host(data + 0x600)) = 0x42; },
     [](const Regs& r) { return at<uint8_t>(host(data + 0x700)) == 0x42 && r.esi == data + 0x601 && r.edi == data + 0x701; }},
    {"rep stosd", {0xf3, 0xab}, {.eax = 0x99887766, .ecx = 3, .edi = data + 0x800}, nullptr,
     [](const Regs& r) {
       return at<uint32_t>(host(data + 0x800)) == 0x99887766 && at<uint32_t>(host(data + 0x808)) == 0x99887766 &&
              r.ecx == 0 && r.edi == data + 0x80c;
     }},
    {"lock xadd", {0xf0, 0x0f, 0xc1, 0x0b}, {.ebx = data + 0x900, .ecx = 5}, [] { at<uint32_t>(host(data + 0x900)) = 10; },
     [](const Regs& r) { return at<uint32_t>(host(data + 0x900)) == 15 && r.ecx == 10; }},
    {"lock cmpxchg", {0xf0, 0x0f, 0xb1, 0x0b}, {.eax = 7, .ebx = data + 0x980, .ecx = 9},
     [] { at<uint32_t>(host(data + 0x980)) = 7; }, [](const Regs& r) { return at<uint32_t>(host(data + 0x980)) == 9 && r.eax == 7; }},
    {"movups", {0x0f, 0x10, 0x03, 0x0f, 0x11, 0x43, 0x10}, {.ebx = data + 0xa00},
     [] { for (int i = 0; i < 16; ++i) at<uint8_t>(host(data + 0xa00 + i)) = 0x30 + i; },
     [](const Regs&) { return !memcmp(reinterpret_cast<void*>(host(data + 0xa00)), reinterpret_cast<void*>(host(data + 0xa10)), 16); }},
    {"x87 fld fstp", {0xdd, 0x03, 0xdd, 0x5b, 0x08}, {.ebx = data + 0xb00}, [] { at<double>(host(data + 0xb00)) = 2.5; },
     [](const Regs&) { return at<double>(host(data + 0xb08)) == 2.5; }},
    // the window is chosen by the whole address, not by the base register alone
    {"edge: high base, low address", {0x8b, 0x43, 0xf8}, {.ebx = edge + 4}, [] { at<uint32_t>(host(edge - 4)) = 0x10101010; },
     [](const Regs& r) { return r.eax == 0x10101010; }},
    {"edge: low base, high address", {0x8b, 0x43, 0x08}, {.ebx = edge - 4}, [] { at<uint32_t>(edge + 4) = 0x20202020; },
     [](const Regs& r) { return r.eax == 0x20202020; }},
    // vector accesses keep the displacement apart from the base register in
    // the IR even on a host without LRCPC2, so these reach the offset path
    {"edge: vector, high base, low address", {0x0f, 0x10, 0x43, 0xe0, 0x66, 0x0f, 0x7e, 0xc0}, {.ebx = edge + 0x10},
     [] { at<uint32_t>(host(edge - 0x10)) = 0x40404040; }, [](const Regs& r) { return r.eax == 0x40404040; }},
    {"edge: vector, low base, high address", {0x0f, 0x10, 0x43, 0x20, 0x66, 0x0f, 0x7e, 0xc0}, {.ebx = edge - 0x10},
     [] { at<uint32_t>(edge + 0x10) = 0x50505050; }, [](const Regs& r) { return r.eax == 0x50505050; }},
    {"high address untouched", {0xa1, 0x00, 0x01, 0x00, 0x09}, {}, [] { at<uint32_t>(high + 0x100) = 0x30303030; },
     [](const Regs& r) { return r.eax == 0x30303030; }},
  };

  unsigned passed = 0, index = 0;
  for (const auto& c : cases) {
    // each case gets its own code, as FEX keeps what it translated at an address
    const uintptr_t entry = code + 0x80 * index;
    if (only >= 0 && int(index) != only) {
      ++index;
      continue;
    }
    ++index;
    memset(reinterpret_cast<void*>(window + data), 0, 0x10000);
    if (c.setup) c.setup();
    memcpy(reinterpret_cast<void*>(code_host + (entry - code)), c.bytes.data(), c.bytes.size());
    at<uint8_t>(code_host + (entry - code) + c.bytes.size()) = 0xf4;
    auto* thread = context->CreateThread();
    auto& state = thread->CurrentFrame->State;
    FEXCore::Core::CPUState::gdt_segment segments[32] {};
    segments[0].D = 1;
    FEXCore::Core::CPUState::SetGDTLimit(&segments[0], 0xfffff);
    state.segment_arrays[0] = state.segment_arrays[1] = segments;
    state.callret_sp = high + 0x8000;
    state.rip = entry;
    state.gregs[REG_RAX] = c.in.eax;
    state.gregs[REG_RBX] = c.in.ebx;
    state.gregs[REG_RCX] = c.in.ecx;
    state.gregs[REG_RDX] = c.in.edx;
    state.gregs[REG_RSI] = c.in.esi;
    state.gregs[REG_RDI] = c.in.edi;
    state.gregs[REG_RSP] = c.in.esp ? c.in.esp : sp;
    context->SetFlagsFromCompactedEFLAGS(thread, 0x202);
    context->ExecuteThread(thread);
    const Regs out {uint32_t(state.gregs[REG_RAX]), uint32_t(state.gregs[REG_RBX]), uint32_t(state.gregs[REG_RCX]),
                    uint32_t(state.gregs[REG_RDX]), uint32_t(state.gregs[REG_RSI]), uint32_t(state.gregs[REG_RDI]),
                    uint32_t(state.gregs[REG_RSP])};
    const bool clean = (state.gregs[REG_RSP] >> 32) == 0 && (state.gregs[REG_RSI] >> 32) == 0 && (state.gregs[REG_RDI] >> 32) == 0;
    const bool ok = clean && c.check(out);
    printf("%s %s\n", ok ? "PASS" : "FAIL", c.name);
    context->DestroyThread(thread);
    if (!ok) return 1;
    ++passed;
  }
  if (expect_fault) {
    printf("FAIL: no access faulted without the window\n");
    return 1;
  }
  printf("FEX window: %u cases through the window at 4 GiB passed\n", passed);
  return 0;
}
