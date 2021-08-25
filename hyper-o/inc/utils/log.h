#include <asm-generic/int-ll64.h>
#include <stdarg.h>
#include <linux/kernel.h>

#define HUGE_ERROR -3
#define YEET -2
#define ERROR -1
#define DEBUG 1
#define INIT 2
#define EXIT 4
//#define LOG_LEVEL (DEBUG | INIT | EXIT)
#define LOG_LEVEL EXIT

//#define jprintf

#define jprintf(level, format, ...) \
	_jprintf(level, format, __func__, ##__VA_ARGS__ );

void _jprintf(u32 level, const char* str, ...);
int log_check(u32 level);

