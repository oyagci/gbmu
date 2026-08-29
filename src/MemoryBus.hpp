#pragma once
#ifndef MEMORYBUS_H
#define MEMORYBUS_H

#include "src/Bios.hpp"
#include "src/Fwd.hpp"
#include "src/IReadWrite.hpp"

#include <iostream>
#include <string>
#include <vector>

#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>

class MemoryBus {
private:
  struct RangedComponent {
    Word begin, end;
    IReadWrite *component;
  };
  std::vector<RangedComponent> _ranged_components;
  Bios *_bios;
  bool _bios_is_enabled;

  friend class boost::serialization::access;
  template <class Archive> void serialize(Archive &ar, const unsigned int) {
    ar &_bios_is_enabled;
  }

public:
  MemoryBus(ComponentsContainer &components);

  void disable_bios();
  void reset(ComponentsContainer &components);

  template <typename T> T read(Word addr) const {
    T ret = 0;
    auto i = sizeof(T);

    if (addr < 0x100 && _bios_is_enabled) {
      return _bios->read(addr);
    } else if (addr >= 0x0200 && addr < 0x0900 && _bios_is_enabled) {
      return _bios->read(addr - 0x0100);
    }

    for (const auto &range : _ranged_components) {
      if (addr >= range.begin and addr <= range.end) {
        while (i-- > 0) {
          ret |= range.component->read(addr + i) >> (i * 8);
        }
        break;
      }
    }
    return ret;
  }

  Byte _sb = 0;
  // Link-port output, mirrored into a bounded buffer so headless drivers can
  // read what a test ROM printed. ponytail: 4K cap, grow it if a ROM says more.
  std::string serial_log;
  template <typename T> void write(Word addr, T v) {
    auto i = sizeof(T);
    if (addr == 0xff01) {
      _sb = v;
    }
    if (addr == 0xff02 && v == 0x81) {
      std::cerr << _sb;
      if (serial_log.size() < 4096)
        serial_log += static_cast<char>(_sb);
    }

    if (addr == 0xFF50) {
      disable_bios();
    }

    for (const auto &range : _ranged_components) {
      if (addr >= range.begin and addr <= range.end) {
        while (i-- > 0) {
          range.component->write(addr + i, v >> (i * 8));
        }
        break;
      }
    }
  }
};

#endif /* MEMORYMAP_H */
