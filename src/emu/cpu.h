#ifndef __CPU_H_
#define __CPU_H_

#include "../risc-io.h"

#include <limits.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>

#define DefaultMemSize      0x00100000
#define DefaultDisplayStart 0x000E7F00

#define FreeListStart 0x190
#define HeapOrg 0x174
#define HeapLim 0x178

#define ROMStart      0xFFFFF800
#define ROMWords     512
#define IOStart      0xFFFFFFC0

#define TRACE_SIZE 500

// #define RV64

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

typedef struct Trace {
  char file[20];
  uint32_t pos;
  uint32_t pc;
  uint8_t file_pos; // used to index into `file`
} Trace;


typedef struct CPU {
  ureg_t pc;
  ureg_t *registers;
  word_t CSR[4096];
  word_t ROM[ROMWords];
  word_t *RAM;

  uint32_t mem_size;
  uint8_t num_regs;
  uint32_t display_start;

  uint32_t current_tick;
  uint32_t mouse;
  uint8_t  key_buf[16];
  uint32_t key_cnt;
  uint32_t switches;

  uint32_t progress;
  uint64_t num_insts; // count number of instructions run
  uint32_t watch_mem; // memory location to "watch"; ie trigger ebreak upon write
  bool     logging;

  const struct RISC_LED *leds;
  const struct RISC_Serial *serial;
  uint32_t spi_selected;
  const struct RISC_SPI *spi[4];
  const struct RISC_Clipboard *clipboard;

  int fb_width;   // words
  int fb_height;  // lines
  struct Damage damage;

  // used for creating a stack trace
  Trace *stack_trace;
  uint16_t stack_index;
} CPU;

uint32_t riscv_load_io(CPU *machine, uint32_t address);
void riscv_store_io(CPU *machine, uint32_t address, uint32_t value);

// TODO Make memory access circular
word_t riscv_load(CPU *machine, addr_t addr);
void riscv_store(CPU *machine, uint32_t address, word_t value);
void riscv_update_damage(CPU *machine, int w);

// IO functions
void riscv_set_leds(CPU *machine, const struct RISC_LED *leds);
void riscv_set_serial(CPU *machine, const struct RISC_Serial *serial);
void riscv_set_spi(CPU *machine, int index, const struct RISC_SPI *spi);
void riscv_set_clipboard(CPU *machine, const struct RISC_Clipboard *clipboard);
void riscv_set_switches(CPU *machine, int switches);
void riscv_set_time(CPU *machine, uint32_t tick);
void riscv_set_logging(CPU *machine, bool log);
void riscv_mouse_moved(CPU *machine, int mouse_x, int mouse_y);
void riscv_mouse_button(CPU *machine, int button, bool down);
void riscv_keyboard_input(CPU *machine, uint8_t *scancodes, uint32_t len);
void riscv_print_trace(CPU *machine);
void write_log(bool logging, const char *format, ...);

uint32_t *riscv_get_framebuffer_ptr(CPU *machine) ;
struct Damage riscv_get_framebuffer_damage(CPU *machine);

void riscv_reset(CPU *machine);

#endif // __CPU_H_
