#ifndef RISC_IO_H
#define RISC_IO_H

#include <stdint.h>

// This is the standard size of the framebuffer, can be overridden.
#define RISC_FRAMEBUFFER_WIDTH 1024
#define RISC_FRAMEBUFFER_HEIGHT 768

struct RISC_Serial {
  uint32_t (*read_status)(const struct RISC_Serial *);
  uint32_t (*read_data)(const struct RISC_Serial *);
  void (*write_data)(const struct RISC_Serial *, uint32_t);
};

struct RISC_SPI {
  uint32_t (*read_data)(const struct RISC_SPI *);
  void (*write_data)(const struct RISC_SPI *, uint32_t);
};

struct RISC_Clipboard {
  void (*write_control)(const struct RISC_Clipboard *, uint32_t);
  uint32_t (*read_control)(const struct RISC_Clipboard *);
  void (*write_data)(const struct RISC_Clipboard *, uint32_t);
  uint32_t (*read_data)(const struct RISC_Clipboard *);
};

struct RISC_LED {
  void (*write)(const struct RISC_LED *, uint32_t);
};

struct Damage {
  int x1, x2, y1, y2;
};

#endif  // RISC_IO_H
