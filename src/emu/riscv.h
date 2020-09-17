#ifndef __RISCV_H_
#define __RISCV_H_

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "../risc-io.h"

// Defines whether to use 64-bit or 32-bit
//#define RV64

#ifdef RV64
#define MAX_VALUE LONG_MAX
#else
#define MAX_VALUE INT_MAX
#endif

#define DefaultMemSize      0x00100000
#define DefaultDisplayStart 0x000E7F00

#define ROMStart     0xFFFFF800
#define ROMWords     512
#define IOStart      0xFFFFFFC0

typedef uint32_t word_t;
typedef uint64_t dword_t;
typedef uint8_t byte_t;

#ifdef RV64
typedef uint64_t ureg_t;
typedef int64_t reg_t;
typedef uint64_t addr_t;
#else
typedef uint32_t ureg_t;
typedef int32_t reg_t;
typedef uint32_t addr_t;
#endif

typedef struct RV {
  ureg_t pc;
  ureg_t registers[32];
  word_t ROM[ROMWords];
  byte_t *RAM;

  uint32_t mem_size;
  uint32_t display_start;

  uint32_t progress;
  uint32_t current_tick;
  uint32_t mouse;
  uint8_t  key_buf[16];
  uint32_t key_cnt;
  uint32_t switches;

  const struct RISC_LED *leds;
  const struct RISC_Serial *serial;
  uint32_t spi_selected;
  const struct RISC_SPI *spi[4];
  const struct RISC_Clipboard *clipboard;

  int fb_width;   // words
  int fb_height;  // lines
  struct Damage damage;
} RISC_V;

RISC_V *riscv_new();
void riscv_reset(RISC_V *machine);

// IO functions
void riscv_set_leds(RISC_V *risc, const struct RISC_LED *leds);
void riscv_set_serial(RISC_V *machine, const struct RISC_Serial *serial);
void riscv_set_spi(RISC_V *machine, int index, const struct RISC_SPI *spi);
void riscv_set_clipboard(RISC_V *machine, const struct RISC_Clipboard *clipboard);
void riscv_set_switches(RISC_V *machine, int switches);
void riscv_set_time(RISC_V *machine, uint32_t tick);
void riscv_mouse_moved(RISC_V *machine, int mouse_x, int mouse_y);
void riscv_mouse_button(RISC_V *machine, int button, bool down);
void riscv_keyboard_input(RISC_V *machine, uint8_t *scancodes, uint32_t len);

uint32_t *riscv_get_framebuffer_ptr(RISC_V *machine);
struct Damage riscv_get_framebuffer_damage(RISC_V *machine);

ureg_t *riscv_read_register(RISC_V *machine, uint8_t reg);
void riscv_write_register(RISC_V *machine, uint8_t reg, ureg_t value);
void riscv_write_pc(RISC_V *machine, ureg_t value);

void riscv_print_state(RISC_V *machine);
void riscv_execute(RISC_V *machine, uint32_t cycles);

#endif // __RISCV_H_
