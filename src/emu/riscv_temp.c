#include "riscv.h"
#include "utils.h"

#include <stdbool.h>
#include <string.h>
#include <stdarg.h>


const uint32_t NUM_REGS = 32; 
const uint32_t SIZE_MEM = DefaultMemSize;

bool jumped = false;
bool logging = true;

static const uint32_t program[ROMWords] = {
#include "bootloader.inc"
};

void write_log(const char *format, ...)
{
  va_list args;
  va_start(args, format);

  if(logging)
    vprintf(format, args);

  va_end(args);
}

// TODO
RISC_V *riscv_new() {
  RISC_V *machine = malloc(sizeof(RISC_V));
  if (machine == NULL)
    exit(2);
  machine->mem_size = DefaultMemSize;

  machine->display_start = DefaultDisplayStart;
  machine->fb_width = RISC_FRAMEBUFFER_WIDTH / 32;
  machine->fb_height = RISC_FRAMEBUFFER_HEIGHT;
  machine->damage = (struct Damage){
    .x1 = 0,
    .y1 = 0,
    .x2 = machine->fb_width - 1,
    .y2 = machine->fb_height - 1
  };
  machine->registers[0] = 0;
  riscv_reset(machine);
  machine->RAM = malloc(SIZE_MEM * sizeof(byte_t));
  memcpy(machine->ROM, program, sizeof(machine->ROM));
  return machine;
}

void riscv_set_leds(RISC_V *machine, const struct RISC_LED *leds) {
  machine->leds = leds;
}

void riscv_set_serial(RISC_V *machine, const struct RISC_Serial *serial) {
  machine->serial = serial;
}

void riscv_set_spi(RISC_V *machine, int index, const struct RISC_SPI *spi) {
  if (index == 1 || index == 2) {
    machine->spi[index] = spi;
  }
}

void riscv_set_clipboard(RISC_V *machine, const struct RISC_Clipboard *clipboard) {
  machine->clipboard = clipboard;
}

void riscv_set_switches(RISC_V *machine, int switches) {
  machine->switches = switches;
}

void riscv_set_time(RISC_V *machine, uint32_t tick) {
  machine->current_tick = tick;
}

void riscv_mouse_moved(RISC_V *machine, int mouse_x, int mouse_y) {
  if (mouse_x >= 0 && mouse_x < 4096) {
    machine->mouse = (machine->mouse & ~0x00000FFF) | mouse_x;
  }
  if (mouse_y >= 0 && mouse_y < 4096) {
    machine->mouse = (machine->mouse & ~0x00FFF000) | (mouse_y << 12);
  }
}

void riscv_mouse_button(RISC_V *machine, int button, bool down) {
  if (button >= 1 && button < 4) {
    uint32_t bit = 1 << (27 - button);
    if (down) {
      machine->mouse |= bit;
    } else {
      machine->mouse &= ~bit;
    }
  }
}

void riscv_keyboard_input(RISC_V *machine, uint8_t *scancodes, uint32_t len) {
  if (sizeof(machine->key_buf) - machine->key_cnt >= len) {
    memmove(&machine->key_buf[machine->key_cnt], scancodes, len);
    machine->key_cnt += len;
  }
}

uint32_t *riscv_get_framebuffer_ptr(RISC_V *machine) {
  // Technically unsafe...
  return (uint32_t*)&machine->RAM[machine->display_start/4];
}

struct Damage riscv_get_framebuffer_damage(RISC_V *machine) {
  struct Damage dmg = machine->damage;
  machine->damage = (struct Damage){
    .x1 = machine->fb_width,
    .x2 = 0,
    .y1 = machine->fb_height,
    .y2 = 0
  };
  return dmg;
}

void riscv_reset(RISC_V *machine) {
  machine->pc = ROMStart;
}

// -- CPU --

uint8_t riscv_get_funct3(uint32_t instruction) {
  return (instruction >> 12) & 0x7;
}

uint8_t riscv_get_funct7(uint32_t instruction) {
  return (instruction >> 25) & 0x7F;
}

uint8_t riscv_get_rs2(uint32_t instruction) {
  return (instruction >> 20) & 0x1F;
}

uint8_t riscv_get_shamt(uint32_t instruction) {
  return riscv_get_rs2(instruction);
}

uint8_t riscv_get_rs1(uint32_t instruction) {
  return (instruction >> 15) & 0x1F;
}

uint8_t riscv_get_rd(uint32_t instruction) { return (instruction >> 7) & 0x1F; }

int32_t riscv_get_imm_typej(uint32_t instruction) {
  uint32_t imm20 = (instruction >> 31) & 1;
  uint32_t imm11 = (instruction >> 20) & 1;
  uint32_t imm10_1 = (instruction >> 21) & 0x3FF;
  uint32_t imm19_12 = (instruction >> 12) & 0xFF;
  return (int32_t)(((imm20 << 20) | (imm19_12 << 12) | (imm11 << 11) |
                   (imm10_1 << 1)) << (32-21)) >> (32-21);
}

int32_t riscv_get_imm_types(uint32_t instruction) {
  int32_t imm11_5 = (instruction >> 25) & 0x3F;
  uint32_t imm4_0 = (instruction >> 7) & 0x1F;
  return (int32_t)((imm11_5 << 5) | (imm4_0));
}

int32_t riscv_get_imm_typeb(uint32_t instruction) {
  uint32_t imm12 = (instruction >> 31) & 1;
  uint32_t imm11 = (instruction >> 7) & 1;
  uint32_t imm10_5 = (instruction >> 25) & 0x3F;
  uint32_t imm4_1 = (instruction >> 8) & 0xF;
  return (int32_t)(((imm12 << 12) | (imm11 << 11) | (imm10_5 << 5) |
                   (imm4_1 << 1)) << (32-13)) >> (32-13);
}

int32_t riscv_get_imm_typeu(uint32_t instruction) {
  return (instruction & 0xFFFFE000);
}

int32_t riscv_get_imm_typei(uint32_t instruction) {
  printf("%x", instruction >> 20);
  return (instruction&0x80000000) ? 0xFFFFF000 | (instruction >> 20) : instruction >> 20;
}

uint8_t riscv_get_opcode(uint32_t instruction) { return instruction & 0x7F; }


#ifdef RV64
void riscv_immediate_arithmetic_word(RISC_V *machine, uint32_t instruction) {
  uint8_t imm = riscv_get_imm_typei(instruction);
  uint8_t funct3 = riscv_get_funct3(instruction);
  
  ureg_t *rs1 = riscv_read_register(machine, riscv_get_rs1(instruction));
  uint8_t rd = riscv_get_rd(instruction);
  uint8_t shamt = riscv_get_shamt(instruction);
  uint8_t funct7 = riscv_get_funct7(instruction);

  // TODO add overflow bit sets etcetc
  switch (funct3) {
  case 0x0: // ADDI
    riscv_write_register(machine, rd, (word_t)add(*rs1, imm));
    break;
  case 0x1: // SLLI
    riscv_write_register(machine, rd, (word_t)(*rs1 << shamt));
    break;
  case 0x5:               // SRLI / SRAI
    if (funct7 == 0x20) { // SRAI
      // technically whether this is arithmetic is implementation-defined
      // TODO make this better
      riscv_write_register(machine, rd, (word_t)((reg_t)(*rs1) >> shamt));
    } else { // SRLI
      riscv_write_register(machine, rd, (word_t)(*rs1 >> shamt));
    }
    break;
  default:
    printf("Unsupported funct3 in imm arithmetic word: %x", funct3);
    break;
  }
}
#endif

void riscv_immediate_arithmetic(RISC_V *machine, uint32_t instruction) {
  int8_t imm = riscv_get_imm_typei(instruction);
  uint8_t funct3 = riscv_get_funct3(instruction);
  uint8_t rs1_name = riscv_get_rs1(instruction);
  ureg_t *rs1 = riscv_read_register(machine, rs1_name);
  uint8_t rd = riscv_get_rd(instruction);
  uint8_t shamt = riscv_get_shamt(instruction);
  uint8_t funct7 = riscv_get_funct7(instruction);

  // TODO add overflow bit sets etcetc
  switch (funct3) {
  case 0x0: // ADDI 
    write_log("addi x%d, x%d, %d", rd, rs1_name, imm);
    riscv_write_register(machine, rd, *rs1 + imm);
    break;
  case 0x1: // SLLI
    write_log("slli x%d, x%d, %d", rd, rs1_name, imm);
    riscv_write_register(machine, rd, *rs1 << shamt);
    break;
  case 0x2: // SLTI
    write_log("slti x%d, x%d, %d", rd, rs1_name, imm);
    riscv_write_register(machine, rd, (reg_t)(*rs1) < (reg_t)(imm));
    break;
  case 0x3: // SLTIU
    write_log("sltiu x%d, x%d, %d", rd, rs1_name, imm);
    riscv_write_register(machine, rd, *rs1 < imm);
    break;
  case 0x4: // XORI
    write_log("xori x%d, x%d, %d", rd, rs1_name, imm);
    riscv_write_register(machine, rd, *rs1 ^ imm);
    break;
  case 0x5:               // SRLI / SRAI
    if (funct7 == 0x20) { // SRAI
      // technically whether this is arithmetic is implementation-defined
      // TODO make this better
      write_log("srai x%d, x%d, %d", rd, rs1_name, imm);
      riscv_write_register(machine, rd, (reg_t)(*rs1) >> shamt);
    } else { // SRLI
      write_log("srli x%d, x%d, %d", rd, rs1_name, imm);
      riscv_write_register(machine, rd, *rs1 >> shamt);
    }
    break;
  case 0x6: // ORI
    write_log("ori x%d, %d, %d", rd, rs1_name, imm);
    riscv_write_register(machine, rd, *rs1 | imm);
    break;
  case 0x7: // ANDI
    write_log("andi x%d, %d, %d", rd, rs1_name, imm);
    riscv_write_register(machine, rd, *rs1 & imm);
    break;
  default:
    write_log("Unsupported funct3 in imm arithmetic: %x", funct3);
    break;
  }
}

void riscv_branch(RISC_V *machine, uint32_t instruction) {
  int8_t imm = riscv_get_imm_typeb(instruction);
  uint8_t funct3 = riscv_get_funct3(instruction);
  uint8_t rs1_name = riscv_get_rs1(instruction);
  uint8_t rs2_name = riscv_get_rs2(instruction);
  ureg_t rs1 = *riscv_read_register(machine, riscv_get_rs1(instruction));
  ureg_t rs2 = *riscv_read_register(machine, riscv_get_rs2(instruction));

  switch (funct3) {
  case 0x0: // BEQ
    write_log("beq x%d, x%d, %d", rs1_name, rs2_name, imm);
    if (rs1 == rs2)
      riscv_write_pc(machine, machine->pc + imm);
    break;
  case 0x1: // BNE
    write_log("bne x%d, x%d, %d", rs1_name, rs2_name, imm);
    if (rs1 != rs2)
      riscv_write_pc(machine, machine->pc + imm);
    break;
  case 0x4: // BLT
    write_log("blt x%d, x%d, %d", rs1_name, rs2_name, imm);
    if ((reg_t)rs1 < (reg_t)rs2)
      riscv_write_pc(machine, machine->pc + imm);
    break;
  case 0x5: // BGE
    write_log("bge x%d, x%d, %d", rs1_name, rs2_name, imm);
    printf("rs1 %d rs2 %d", rs1, rs2);
    if ((reg_t)(rs1) >= (reg_t)(rs2)) {
      printf("branch taken");
      riscv_write_pc(machine, machine->pc + imm);
    }
    break;
  case 0x6: // BLTU
    write_log("bltu x%d, x%d, %d", rs1_name, rs2_name, imm);
    if (rs1 < rs2)
      riscv_write_pc(machine, machine->pc + imm);
    break;
  case 0x7: // BGEU
    write_log("bgeu x%d, x%d, %d", rs1_name, rs2_name, imm);
    if (rs1 >= rs2)
      riscv_write_pc(machine, machine->pc + imm);
    break;
  default:
    write_log("Unsupported funct3 in branch: %x", funct3);
    break;
  }
}

void riscv_arithmetic(RISC_V *machine, uint32_t instruction) {
  uint8_t funct3 = riscv_get_funct3(instruction);
  uint8_t rs1_name = riscv_get_rs1(instruction);
  uint8_t rs2_name = riscv_get_rs2(instruction);
  ureg_t *rs1 = riscv_read_register(machine, riscv_get_rs1(instruction));
  ureg_t *rs2 = riscv_read_register(machine, riscv_get_rs2(instruction));
  uint8_t rd = riscv_get_rd(instruction);
  uint8_t funct7 = riscv_get_funct7(instruction);

  // TODO merge with arithmetic_immediate
  if (funct7 == 1) { // mul/div
    switch (funct3) {
      case 0x0: // mul
        write_log("mul x%d, x%d, x%d", rd, rs1_name, rs2_name);
        riscv_write_register(machine, rd, *rs1 * *rs2);
        break;
      case 0x1: // mulh
        write_log("mulh x%d, x%d, x%d", rd, rs1_name, rs2_name);
        riscv_write_register(machine, rd, ((int64_t)(*rs1 * *rs2)) << 32);
        break;
      case 0x2: // mulhsu
        write_log("mulhsu x%d, x%d, x%d", rd, rs1_name, rs2_name);
        riscv_write_register(machine, rd, ((int64_t)((reg_t)*rs1 * *rs2)) << 32);
        break;
      case 0x3: // mulhu
        write_log("mulhu x%d, x%d, x%d", rd, rs1_name, rs2_name);
        riscv_write_register(machine, rd, ((uint64_t)(*rs1 * *rs2)) << 32);
        break;
      case 0x4: // div
        write_log("div x%d, x%d, x%d", rd, rs1_name, rs2_name);
        riscv_write_register(machine, rd, (reg_t)*rs1 / (reg_t)*rs2);
        break;
      case 0x5: // divu
        write_log("divu x%d, x%d, x%d", rd, rs1_name, rs2_name);
        riscv_write_register(machine, rd, *rs1 / *rs2);
        break;
      case 0x6: // rem
        write_log("rem x%d, x%d, x%d", rd, rs1_name, rs2_name);
        riscv_write_register(machine, rd, (reg_t)*rs1 % (reg_t)*rs2);
        break;
      case 0x7: // remu
        write_log("remu x%d, x%d, x%d", rd, rs1_name, rs2_name);
        riscv_write_register(machine, rd, *rs1 % *rs2);
        break;
      default:
        write_log("Unsupported funct3 in imm mul arithmetic: %x", funct3);
        break;
    }
  } else {
    switch (funct3) {
    case 0x0:             // ADD/SUB
      if (funct7 == 0x20) { // SUB
        write_log("sub x%d, x%d, x%d", rd, rs1_name, rs2_name);
        riscv_write_register(machine, rd, *rs1 - *rs2);
      } else if (funct7 == 0x0) {// ADD
        write_log("add x%d, x%d, x%d", rd, rs1_name, rs2_name);
        riscv_write_register(machine, rd, *rs1 + *rs2);
      }
      break;
    case 0x1: // SLL
      write_log("sll x%d, x%d, x%d", rd, rs1_name, rs2_name);
      riscv_write_register(machine, rd, *rs1 << *rs2);
      break;
    case 0x2: // SLT
      write_log("slt x%d, x%d, x%d", rd, rs1_name, rs2_name);
      riscv_write_register(machine, rd, (reg_t)(*rs1) < (reg_t)(*rs2));
      break;
    case 0x3: // SLTU
      write_log("sltu x%d, x%d, x%d", rd, rs1_name, rs2_name);
      riscv_write_register(machine, rd, *rs1 < *rs2);
      break;
    case 0x4: // XOR
      write_log("xor x%d, x%d, x%d", rd, rs1_name, rs2_name);
      riscv_write_register(machine, rd, *rs1 ^ *rs2);
      break;
    case 0x5:               // SRL / SRA
      if (funct7 == 0x20) { // SRA
        // technically whether this is arithmetic is implementation-defined
        // TODO make this better
        write_log("sra x%d, x%d, x%d", rd, rs1_name, rs2_name);
        riscv_write_register(machine, rd, (reg_t)(*rs1) >> *rs2);
      } else { // SRL
        write_log("srl x%d, x%d, x%d", rd, rs1_name, rs2_name);
        riscv_write_register(machine, rd, *rs1 >> *rs2);
      }
      break;
    case 0x6: // ORI
      write_log("ori x%d, x%d, x%d", rd, rs1_name, rs2_name);
      riscv_write_register(machine, rd, *rs1 | *rs2);
      break;
    case 0x7: // ANDI
      write_log("andi x%d, x%d, x%d", rd, rs1_name, rs2_name);
      riscv_write_register(machine, rd, *rs1 & *rs2);
      break;
    default:
      write_log("Unsupported funct3 in imm arithmetic: %x", funct3);
      break;
    }
  }
}

#ifdef RV64
void riscv_arithmetic_word(RISC_V *machine, uint32_t instruction) {
  uint8_t funct3 = riscv_get_funct3(instruction);
  ureg_t *rs1 = riscv_read_register(machine, riscv_get_rs1(instruction));
  ureg_t *rs2 = riscv_read_register(machine, riscv_get_rs2(instruction));
  uint8_t rd = riscv_get_rd(instruction);
  uint8_t funct7 = riscv_get_funct7(instruction);

  // TODO merge with arithmetic
  switch (funct3) {
  case 0x0:             // ADD/SUB
    if (funct7 == 0x20) // SUB
      riscv_write_register(machine, rd, (word_t)sub(*rs1, *rs2));
    else if (funct7 == 0x0) // ADD
      riscv_write_register(machine, rd, (word_t)add(*rs1, *rs2));
    break;
  case 0x1: // SLL
    riscv_write_register(machine, rd, (word_t)(*rs1 << *rs2));
    break;
  case 0x5:               // SRL / SRA
    if (funct7 == 0x20) { // SRA
      // technically whether this is arithmetic is implementation-defined
      // TODO make this better
      riscv_write_register(machine, rd, (word_t)((reg_t)(*rs1) >> *rs2));
    } else { // SRL
      riscv_write_register(machine, rd, (word_t)(*rs1 >> *rs2));
    }
    break;
  default:
    write_log("Unsupported funct3 in imm arithmetic: %x", funct3);
    break;
  }
}
#endif

static uint32_t riscv_load_io(RISC_V *machine, uint32_t address) {
  switch (address - IOStart) {
    case 0: {
      // Millisecond counter
      machine->progress--;
      return machine->current_tick;
    }
    case 4: {
      // Switches
      printf("Read switches.\n");
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
        printf("Read of spi %d with value %d\n", machine->spi_selected, spi->read_data(spi));
        return spi->read_data(spi);
      }
      printf("Read of spi %d with value 255\n", machine->spi_selected);
      return 255;
    }
    case 20: {
      // SPI status
      // Bit 0: rx ready
      // Other bits unused
      printf("Read SPI status.\n");
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

byte_t riscv_load_mem(RISC_V *machine, addr_t addr) {
  if (addr < SIZE_MEM) {
    return machine->RAM[addr];
  } else {
    printf("Illegal IO load; must be loaded as a word.");
    return 0;
  }
}

// TODO Make memory access circular
word_t riscv_load_mem_32(RISC_V *machine, addr_t addr) {
  if (addr < SIZE_MEM)
    return riscv_load_mem(machine, addr) |
           riscv_load_mem(machine, addr + 1) << 8 | 
           riscv_load_mem(machine, addr + 2) << 16 | 
           riscv_load_mem(machine, addr + 3) << 24;
  else
    return riscv_load_io(machine, addr);
}

#ifdef RV64
dword_t riscv_load_mem_64(RISC_V *machine, addr_t addr) {
  return riscv_load_mem_32(machine, addr) << 32 |
         riscv_load_mem_32(machine, addr + 4);
}
#endif


void riscv_load(RISC_V *machine, uint32_t instruction) {
  uint32_t imm = riscv_get_imm_typei(instruction);
  uint8_t funct3 = riscv_get_funct3(instruction);
  uint8_t rs1_name = riscv_get_rs1(instruction);
  ureg_t *rs1 = riscv_read_register(machine, riscv_get_rs1(instruction));
  uint8_t rd = riscv_get_rd(instruction);

  switch (funct3) {
  case 0x0: // lb
    write_log("lb x%d, %d(x%d)", rd, imm, rs1_name);
    riscv_write_register(machine, rd, (reg_t)machine->RAM[*rs1 + imm]);
    break;
  case 0x1: // lh
    write_log("lh x%d, %d(x%d)", rd, imm, rs1_name);
    riscv_write_register(machine, rd,
                         (reg_t)(machine->RAM[*rs1 + imm] |
                                 machine->RAM[*rs1 + imm + 1] << 8));
    break;
  case 0x2: // lw
    write_log("lw x%d, %d(x%d)", rd, imm, rs1_name);
    //write_log("load time, addr: %x\n", add(*rs1, imm));
    riscv_write_register(machine, rd,
                         (reg_t)riscv_load_mem_32(machine, *rs1 + imm));
    break;
#ifdef RV64
  case 0x3: // ld
    write_log("ld x%d, %d(x%d)", rd, imm, rs1_name);
    riscv_write_register(machine, rd,
                         (reg_t)riscv_load_mem_64(machine, *rs1 + imm));
    break;
#endif
  case 0x4: // lbu
    write_log("lbu x%d, %d(x%d)", rd, imm, rs1_name);
    riscv_write_register(machine, rd, (reg_t)machine->RAM[*rs1 + imm]);
    break;
  case 0x5: // lhu
    write_log("lhu x%d, %d(x%d)", rd, imm, rs1_name);
    riscv_write_register(machine, rd,
                         machine->RAM[*rs1 + imm] |
                         machine->RAM[*rs1 + imm + 1] << 8);
    break;
#ifdef RV64
  case 0x6: // lwu
    riscv_write_register(machine, rd, riscv_read_mem_32(machine, *rs1 + imm));
    break;
#endif
  default:
    write_log("Unsupported funct3 in load: %x", funct3);
    break;
  }
}

static void riscv_store_io(RISC_V *machine, uint32_t address, uint32_t value) {
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
        printf("Attempted write to spi %d with value %d\n", machine->spi_selected, value);
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
      printf("Set spi mode to %d\n", value & 3);
      machine->spi_selected = value & 3;
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
      write_log("Wrote %0x to undefined IO at address %0x.", value, address);
      break;
  }
}

void riscv_store_mem(RISC_V *machine, uint32_t address, byte_t value) {
  if (address < SIZE_MEM) {
    machine->RAM[address] = value;
  } else {
    write_log("Illegal store to IO, as it is less than a word.");
    //riscv_store_io(machine, address, value);
  }
}

void riscv_store_mem_32(RISC_V *machine, uint32_t address, word_t value) {
  if (address < SIZE_MEM) {
    riscv_store_mem(machine, address + 0,  value        & 0xFF);
    riscv_store_mem(machine, address + 1, (value >> 8)  & 0xFF);
    riscv_store_mem(machine, address + 2, (value >> 16) & 0xFF);
    riscv_store_mem(machine, address + 3, (value >> 24) & 0xFF);
  } else {
    riscv_store_io(machine, address, value);
  }
}

void riscv_store(RISC_V *machine, uint32_t instruction) {
  int32_t imm = riscv_get_imm_types(instruction);
  uint8_t funct3 = riscv_get_funct3(instruction);
  uint8_t rs1_name = riscv_get_rs1(instruction);
  uint8_t rs2_name = riscv_get_rs2(instruction);
  reg_t *rs1 = (reg_t *)riscv_read_register(machine, riscv_get_rs1(instruction));
  ureg_t *rs2 = riscv_read_register(machine, riscv_get_rs2(instruction));

  switch (funct3) {
  case 0x0: // sb
    write_log("sb x%d, %d(x%d)", rs2_name, imm, rs1_name);
    riscv_store_mem(machine, *rs1 + imm, *rs2 & 0xFF);
    break;
  case 0x1: // sh
    write_log("sh x%d, %d(x%d)", rs2_name, imm, rs1_name);
    riscv_store_mem(machine, *rs1 + imm, *rs2 & 0xFF);
    riscv_store_mem(machine, *rs1 + imm + 1, (*rs2 >> 8) & 0xFF);
    break;
  // TODO make this less repetitive
  case 0x2: // sw
    write_log("sw x%d, %d(x%d)", rs2_name, imm, rs1_name);
    riscv_store_mem_32(machine, *rs1 + imm, *rs2);
    break;
#ifdef RV64
  case 0x3:
    write_log("sdw x%d, %d(x%d)", rs2_name, imm, rs1_name);
    machine->RAM[*rs1 + imm] = *rs2 & 0xFF;
    machine->RAM[*rs1 + imm + 1] = (*rs2 >> 8) & 0xFF;
    machine->RAM[*rs1 + imm + 2] = (*rs2 >> 16) & 0xFF;
    machine->RAM[*rs1 + imm + 3] = (*rs2 >> 24) & 0xFF;
    machine->RAM[*rs1 + imm + 4] = (*rs2 >> 32) & 0xFF;
    machine->RAM[*rs1 + imm + 5] = (*rs2 >> 40) & 0xFF;
    machine->RAM[*rs1 + imm + 6] = (*rs2 >> 48) & 0xFF;
    machine->RAM[*rs1 + imm + 7] = (*rs2 >> 56) & 0xFF;
    break;
#endif
  default:
    write_log("Unsupported funct3 in store: %x", funct3);
    break;
  }
}

void riscv_execute_instruction(RISC_V *machine, uint32_t instruction) {

  uint8_t opcode = riscv_get_opcode(instruction);
  ureg_t rd = riscv_get_rd(instruction);
  ureg_t rs1 = riscv_get_rs1(instruction);
  switch (opcode) {
  case 0x3: // loads
    riscv_load(machine, instruction);
    break;
  case 0x13: // Arithmetic with imm RHS
    riscv_immediate_arithmetic(machine, instruction);
    break;
  case 0x17: // auipc
    write_log("auipc x%d, %d", rd, riscv_get_imm_typeu(instruction));
    riscv_write_register(machine, rd,
                         machine->pc + riscv_get_imm_typeu(instruction));
    break;
#ifdef RV64
  case 0x1B: // immw
    riscv_immediate_arithmetic_word(machine, instruction);
    break;
#endif
  case 0x23: // stores
    riscv_store(machine, instruction);
    break;
  case 0x33: // Arithmetic with register RHS
    riscv_arithmetic(machine, instruction);
    break;
  case 0x37: // lui
    write_log("lui x%d, %d", rd, riscv_get_imm_typeu(instruction));
    riscv_write_register(machine, rd, riscv_get_imm_typeu(instruction));
    break;
  case 0x63: // branches
    riscv_branch(machine, instruction);
    break;
#ifdef RV64
  case 0x3B:
    riscv_arithmetic_word(machine, instruction);
    break;
#endif
  case 0x67: // jalr
    write_log("jalr x%d, x%d, %d", rd, rs1, riscv_get_imm_typei(instruction));
    riscv_write_register(machine, rd, machine->pc + 4);
    riscv_write_pc(machine, riscv_get_imm_typei(instruction) + (reg_t)*riscv_read_register(machine, rs1));
    break;
  case 0x6F: // jal
    write_log("jal x%d, %d", rd, riscv_get_imm_typej(instruction));
    riscv_write_register(machine, rd, machine->pc + 4);
    riscv_write_pc(machine, machine->pc + riscv_get_imm_typej(instruction));
    break;
  default:
    write_log("Unsupported opcode %x\n", opcode);
  }
}

void riscv_execute(RISC_V *machine, uint32_t cycles) {
  for (uint32_t i = 0; i < cycles; i++) {
    if (i != 0 && !jumped)
      riscv_write_pc(machine, machine->pc + 4);
    jumped = false;
    word_t instruction = 0;
    if (machine->pc < machine->mem_size) {
      instruction = machine->RAM[machine->pc / 4];
    } else if (machine->pc >= ROMStart) {
      instruction = machine->ROM[(machine->pc - ROMStart) / 4];
    } else {
      write_log("Panic! PC = %0x", machine->pc);
      return;
    }

    if (instruction == 0) {
      write_log("Illegal instruction encountered: %d", instruction);
      return;
    }
    write_log("INSTRUCTION:\t");
    riscv_execute_instruction(machine, instruction);
    write_log(" [%08x]", instruction);
    riscv_print_state(machine);
  }
}

ureg_t *riscv_read_register(RISC_V *machine, uint8_t reg) {
  return &machine->registers[reg];
}

void riscv_write_register(RISC_V *machine, uint8_t reg, ureg_t value) {
  if (reg != 0) {
    machine->registers[reg] = value;
  }
}

void riscv_write_pc(RISC_V *machine, ureg_t value) {
  // check if it actually jumps anywhere
  if (machine->pc != value) {
    jumped = true;
    machine->pc = value;
  }
}

void riscv_print_state(RISC_V *machine) {
  write_log("\nState:\nPC: 0x%x\n", machine->pc);
  for (uint8_t i = 0; i < NUM_REGS; i++) {
    if (machine->registers[i] != 0)
      write_log("Reg %d: 0x%x\n", i, machine->registers[i]);
  }
  for (uint32_t i = 0; i < SIZE_MEM; i += 1) {
    // int32_t m = ((int32_t *)(machine->RAM))[i];
    uint8_t m = machine->RAM[i];
    if (m != 0)
      write_log("Mem %x: 0x%x\n", i, m);
  }
  write_log("\n");
}
