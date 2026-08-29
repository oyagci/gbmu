// gbmu-dbg: headless ROM debugger. Links the emulator without Qt.
//
// The Debugger class is deliberately NOT enabled here: its fetch() busy-waits
// on a lock another thread must clear, which deadlocks a single-threaded
// driver. We drive Gameboy::step() ourselves and use the lock-free half of the
// Debugger API (disassembly, memory dump) for output.

#include "src/Gameboy.hpp"
#include "src/Bios.hpp"
#include "src/MemoryBus.hpp"
#include "src/cpu/Core.hpp"

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <set>
#include <string>

// Gameboy declares `friend class Accessor` for the tests; reuse that hook to
// run the same boot sequence as Gameboy::run(), minus the audio and the loop.
class Accessor {
public:
  static void boot(Gameboy &gb) {
    gb.do_checksum();
    gb.read_type();
    gb._components.bios->set_type(gb._type);
    gb.load_existing_save();
  }
};

namespace {

struct Options {
  std::string rom;
  std::set<Word> breakpoints;
  unsigned long max_steps = 0; // 0 = unlimited
  bool trace = false;
  bool has_dump = false;
  Word dump_addr = 0;
  GbType type = GbType::DEFAULT;
};

void usage(const char *argv0) {
  std::fprintf(stderr,
               "usage: %s <rom.gb> [options]\n"
               "  -b, --break ADDR   break at ADDR (hex), repeatable\n"
               "  -n, --steps N      stop after N instructions\n"
               "  -t, --trace        disassemble every instruction\n"
               "  -d, --dump ADDR    dump 160 bytes at ADDR when stopped\n"
               "      --dmg          force DMG (default: auto-detect)\n"
               "      --cgb          force CGB\n"
               "\n"
               "exit: 0 stopped/passed, 1 failed, 2 step limit, 3 usage\n"
               "serial output goes to stderr as the ROM writes it\n",
               argv0);
}

// Returns false on a bad argument; `ok` distinguishes -h from an error.
bool parse(int ac, char **av, Options &o, bool &ok) {
  ok = false;
  for (int i = 1; i < ac; i++) {
    std::string a = av[i];
    auto next = [&](const char *what) -> const char * {
      if (i + 1 >= ac) {
        std::fprintf(stderr, "%s: missing %s\n", a.c_str(), what);
        return nullptr;
      }
      return av[++i];
    };
    if (a == "-h" || a == "--help") {
      ok = true;
      return false;
    } else if (a == "-b" || a == "--break") {
      const char *v = next("address");
      if (!v)
        return false;
      o.breakpoints.insert(std::strtoul(v, nullptr, 16));
    } else if (a == "-n" || a == "--steps") {
      const char *v = next("count");
      if (!v)
        return false;
      o.max_steps = std::strtoul(v, nullptr, 0);
    } else if (a == "-d" || a == "--dump") {
      const char *v = next("address");
      if (!v)
        return false;
      o.dump_addr = std::strtoul(v, nullptr, 16);
      o.has_dump = true;
    } else if (a == "-t" || a == "--trace") {
      o.trace = true;
    } else if (a == "--dmg") {
      o.type = GbType::DMG;
    } else if (a == "--cgb") {
      o.type = GbType::CGB;
    } else if (!a.empty() && a[0] == '-') {
      std::fprintf(stderr, "unknown option: %s\n", a.c_str());
      return false;
    } else if (o.rom.empty()) {
      o.rom = a;
    } else {
      std::fprintf(stderr, "unexpected argument: %s\n", a.c_str());
      return false;
    }
  }
  if (o.rom.empty()) {
    std::fprintf(stderr, "missing rom path\n");
    return false;
  }
  return true;
}

void print_trace(Debugger &dbg) {
  dbg.reset();
  const auto &pool = dbg.get_instruction_pool();
  if (pool.empty())
    return;
  const auto &in = pool.front();
  std::printf("%04X  ", in.pc);
  for (int i = 0; i < 3; i++) {
    if (i < in.size)
      std::printf("%02X ", in.value[i] & 0xFF);
    else
      std::printf("   ");
  }
  std::printf(" %s\n", in.instr ? in.instr : "??");
}

void print_state(const Gameboy &gb) {
  const auto &c = *gb.components().core;
  std::printf("PC:%04X SP:%04X AF:%04X BC:%04X DE:%04X HL:%04X  LY:%02X\n",
              c.pc(), c.sp(), c.af().word, c.bc().word, c.de().word,
              c.hl().word, gb.components().mem_bus->read<Byte>(0xFF44));
}

void print_dump(Debugger &dbg, Word addr) {
  const auto bytes = dbg.get_memory_dump(addr);
  for (std::size_t i = 0; i < bytes.size(); i++) {
    if (i % 16 == 0)
      std::printf("%s%04X ", i ? "\n" : "", static_cast<unsigned>(addr + i));
    std::printf(" %02X", bytes[i]);
  }
  std::printf("\n");
}

// Blargg test ROMs print their verdict over the link port.
int serial_verdict(const std::string &log) {
  if (log.find("Passed") != std::string::npos)
    return 0;
  if (log.find("Failed") != std::string::npos)
    return 1;
  return -1;
}

}

int main(int ac, char **av) {
  Options o;
  bool ok = false;
  if (!parse(ac, av, o, ok)) {
    usage(av[0]);
    return ok ? 0 : 3;
  }

  // The cartridge is parsed by Gameboy's constructor, so a bad ROM throws
  // before we have an object -- build it inside the guard.
  std::unique_ptr<Gameboy> owner;
  try {
    owner.reset(new Gameboy(o.rom, o.type));
    owner->set_free_running(true); // never pace the run off the sound card
    Accessor::boot(*owner);
  } catch (const std::exception &e) {
    std::fprintf(stderr, "%s: %s\n", o.rom.c_str(), e.what());
    return 1;
  }
  Gameboy &gb = *owner;

  Debugger &dbg = gb.get_debugger();
  dbg.set_instruction_pool_size(0); // one instruction per trace line
  auto &bus = *gb.components().mem_bus;

  const char *reason = "step limit";
  int rc = 2;
  std::size_t seen_serial = 0;
  for (unsigned long n = 0; !o.max_steps || n < o.max_steps; n++) {
    if (o.trace)
      print_trace(dbg);
    if (!o.breakpoints.empty() &&
        o.breakpoints.count(gb.components().core->pc())) {
      reason = "breakpoint";
      rc = 0;
      break;
    }
    gb.step();
    if (bus.serial_log.size() != seen_serial) {
      seen_serial = bus.serial_log.size();
      int verdict = serial_verdict(bus.serial_log);
      if (verdict >= 0) {
        reason = verdict == 0 ? "serial: passed" : "serial: failed";
        rc = verdict;
        break;
      }
    }
  }

  std::printf("\nstopped: %s\n", reason);
  print_state(gb);
  if (o.has_dump)
    print_dump(dbg, o.dump_addr);
  return rc;
}
