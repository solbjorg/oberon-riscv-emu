#ifndef __UTILS_H_
#define __UTILS_H_

#include "riscv.h"

ureg_t add(reg_t lhs, reg_t rhs) { return (lhs + rhs) % UINT_MAX; }
ureg_t sub(reg_t lhs, reg_t rhs) { return (lhs - rhs) % MAX_VALUE; }
// TODO check if right-shift should wrap

#endif // __UTILS_H_
