#include "cpu.h"


uint32_t riscv_load_io(CPU *machine, uint32_t address) {
  switch (address - IOStart) {
    case 0: {
      // Millisecond counter
      machine->progress--;
      return machine->current_tick;
    }
    case 4: {
      // Switches
      //printf("Read switches.\n");
      return machine->switches;
    }
    case 8: {
      // RS232 data
      if (machine->serial) {
        return machine->serial->read_data(machine->serial);
      }
      return 0;
    }
    case 12: {
      // RS232 status
      if (machine->serial) {
        return machine->serial->read_status(machine->serial);
      }
      return 0;
    }
    case 16: {
      // SPI data
      const struct RISC_SPI *spi = machine->spi[machine->spi_selected];
      if (spi != NULL) {
        //printf("Read of spi %d with value %d\n", machine->spi_selected, spi->read_data(spi));
        return spi->read_data(spi);
      }
      //printf("Read of spi %d with value 255\n", machine->spi_selected);
      return 255;
    }
    case 20: {
      // SPI status
      // Bit 0: rx ready
      // Other bits unused
      //printf("Read SPI status.\n");
      return 1;
    }
    case 24: {
      // Mouse input / keyboard status
      uint32_t mouse = machine->mouse;
      if (machine->key_cnt > 0) {
        mouse |= 0x10000000;
      } else {
        machine->progress--;
      }
      return mouse;
    }
    case 28: {
      // Keyboard input
      if (machine->key_cnt > 0) {
        uint8_t scancode = machine->key_buf[0];
        machine->key_cnt--;
        memmove(&machine->key_buf[0], &machine->key_buf[1], machine->key_cnt);
        return scancode;
      }
      return 0;
    }
    case 40: {
      // Clipboard control
      if (machine->clipboard) {
        return machine->clipboard->read_control(machine->clipboard);
      }
      return 0;
    }
    case 44: {
      // Clipboard data
      if (machine->clipboard) {
        return machine->clipboard->read_data(machine->clipboard);
      }
      return 0;
    }
    default: {
      return 0;
    }
  }
}

void riscv_store_io(CPU *machine, uint32_t address, uint32_t value) {
  switch (address - IOStart) {
    case 4: {
      // LED control
      if (machine->leds) {
        machine->leds->write(machine->leds, value);
      }
      break;
    }
    case 8: {
      // RS232 data
      if (machine->serial) {
        machine->serial->write_data(machine->serial, value);
      }
      break;
    }
    case 16: {
      // SPI write
      const struct RISC_SPI *spi = machine->spi[machine->spi_selected];
      if (spi != NULL) {
        //printf("Attempted write to spi %d with value %d\n", machine->spi_selected, value);
        spi->write_data(spi, value);
      }
      break;
    }
    case 20: {
      // SPI control
      // Bit 0-1: slave select
      // Bit 2:   fast mode
      // Bit 3:   netwerk enable
      // Other bits unused
      //printf("Set spi mode to %d\n", value & 3);
      machine->spi_selected = value & 3;
      break;
    }
    case 32: {
      if (value == 0) {
        if (machine->stack_index > 0) {
          machine->stack_index--;
          Trace *trace = &machine->stack_trace[machine->stack_index];
          //printf("Function return to pos %d in %s\n", trace->pos, trace->file);
          machine->stack_trace[machine->stack_index] = (Trace){ .file = "", .pos = 0, .file_pos = 0 };
        } else {
          printf("ERROR: Illegal stack trace pop.\n");
        }
      } else {
        if (machine->stack_index >= TRACE_SIZE) {
          printf("ERROR: Illegal stack trace push; stack full.\n");
        }
        else {
          switch(value >> 24) {
            case 0xAA: {
              Trace *trace = &machine->stack_trace[machine->stack_index];
              trace->file[trace->file_pos++] = (char)value;
              break;
            }
            case 0xBB: {
              riscv_print_trace(machine);
              machine->stack_index = 0;
              break;
            }
            case 0xCC: {
              Trace *trace = &machine->stack_trace[machine->stack_index];
              trace->file[trace->file_pos] = '\0';
              trace->file_pos = 0;
              trace->pos = value % 0x1000000;
              //printf("Function call at pos %d in %s\n", trace->pos, trace->file);
              machine->stack_index++;
              break;
            }
            default:
              printf("Unknown stack trace push.");
          }
        }
      }
      break;
    }
    case 40: {
      // Clipboard control
      if (machine->clipboard) {
        machine->clipboard->write_control(machine->clipboard, value);
      }
      break;
    }
    case 44: {
      // Clipboard data
      if (machine->clipboard) {
        machine->clipboard->write_data(machine->clipboard, value);
      }
      break;
    }
    default:
      printf("Wrote %0x to undefined IO at address %0x.", value, address);
      break;
  }
}

// TODO Make memory access circular
word_t riscv_load(CPU *machine, addr_t addr) {
  if (addr < machine->mem_size) {
    //printf("loading %x from addr %x", machine->RAM[addr/4], addr);
    return machine->RAM[addr/4];
  }
  else
    return riscv_load_io(machine, addr);
}

void riscv_store(CPU *machine, uint32_t address, word_t value) {
  if (address < machine->display_start) {
    //printf("Store of %x to %x", value, address);
    machine->RAM[address/4] = value;
  } else if (address < machine->mem_size) {
    machine->RAM[address/4] = value;
    riscv_update_damage(machine, address/4 - machine->display_start/4);
  } else {
    riscv_store_io(machine, address, value);
  }
}

void riscv_update_damage(CPU *machine, int w) {
  int row = w / machine->fb_width;
  int col = w % machine->fb_width;
  if (row < machine->fb_height) {
    if (col < machine->damage.x1) {
      machine->damage.x1 = col;
    }
    if (col > machine->damage.x2) {
      machine->damage.x2 = col;
    }
    if (row < machine->damage.y1) {
      machine->damage.y1 = row;
    }
    if (row > machine->damage.y2) {
      machine->damage.y2 = row;
    }
  }
}

// IO functions
void riscv_set_leds(CPU *machine, const struct RISC_LED *leds) {
  machine->leds = leds;
}

void riscv_set_serial(CPU *machine, const struct RISC_Serial *serial) {
  machine->serial = serial;
}

void riscv_set_spi(CPU *machine, int index, const struct RISC_SPI *spi) {
  if (index == 1 || index == 2) {
    machine->spi[index] = spi;
  }
}

void riscv_set_clipboard(CPU *machine, const struct RISC_Clipboard *clipboard) {
  machine->clipboard = clipboard;
}

void riscv_set_switches(CPU *machine, int switches) {
  machine->switches = switches;
}

void riscv_set_time(CPU *machine, uint32_t tick) {
  machine->current_tick = tick;
}

void riscv_set_logging(CPU *machine, bool log) {
  machine->logging = log;
}

void riscv_mouse_moved(CPU *machine, int mouse_x, int mouse_y) {
  if (mouse_x >= 0 && mouse_x < 4096) {
    machine->mouse = (machine->mouse & ~0x00000FFF) | mouse_x;
  }
  if (mouse_y >= 0 && mouse_y < 4096) {
    machine->mouse = (machine->mouse & ~0x00FFF000) | (mouse_y << 12);
  }
}

void riscv_mouse_button(CPU *machine, int button, bool down) {
  if (button >= 1 && button < 4) {
    uint32_t bit = 1 << (27 - button);
    if (down) {
      machine->mouse |= bit;
    } else {
      machine->mouse &= ~bit;
    }
  }
}

void riscv_keyboard_input(CPU *machine, uint8_t *scancodes, uint32_t len) {
  if (sizeof(machine->key_buf) - machine->key_cnt >= len) {
    memmove(&machine->key_buf[machine->key_cnt], scancodes, len);
    machine->key_cnt += len;
  }
}

uint32_t *riscv_get_framebuffer_ptr(CPU *machine) {
  // Technically unsafe...
  return (uint32_t*)&machine->RAM[machine->display_start/4];
}

struct Damage riscv_get_framebuffer_damage(CPU *machine) {
  struct Damage dmg = machine->damage;
  machine->damage = (struct Damage){
    .x1 = machine->fb_width,
    .x2 = 0,
    .y1 = machine->fb_height,
    .y2 = 0
  };
  return dmg;
}

void riscv_reset(CPU *machine) {
  machine->pc = ROMStart;
}

void riscv_print_trace(CPU *machine) {
  for (uint16_t i = 0; i < machine->stack_index; i++) {
    printf("Entering from module %s at position %d\n", machine->stack_trace[i].file, machine->stack_trace[i].pos);
  }
  //printf("Mem:\n");
  //for (int i=0; i<machine->mem_size/4; i++) { if (machine->RAM[i] != 0) printf("%x: 0x%x\n",i*4,machine->RAM[i]); } printf("\n"); 
}
