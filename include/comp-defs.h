#ifndef __COMP_DEFS_H
#define __COMP_DEFS_H 1

#include <stddef.h>

#define __head __attribute__((section(".head.text"), aligned(2)))
#define __text __attribute__((section(".text"), aligned(2)))
#define __noreturn __attribute__((noreturn))
#define __interrupt __attribute__((signal))
#define __noinline __attribute__((noinline))
#define __packed __attribute__((packed))
#define __unused __attribute__((unused))
#define __used __attribute__((used))

#endif
