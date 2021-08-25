#include <asm/cpu_entry_area.h>
#include <linux/slab.h> // mem functions    
#include <asm/desc.h>
#include <asm/io.h> // virt_to_phys ---> use __pa ???

#include "inc/utils/log.h"

int log_check(__u32 level)
{
	return (LOG_LEVEL & level);
}

void _jprintf(__u32 level, const char* str, ...)
{
	char format[420];
	char log_message[420];
	va_list args;

	if (!log_check(level))
	{
		return;
	}

	if (level == HUGE_ERROR)
	{
		printk(KERN_INFO "\n");
	}
	if (level == ERROR || level == HUGE_ERROR)
	{
		snprintf(format, 420, "{%d %llx} %s%s", smp_processor_id(), ((__u64)current) & 0xffff, "[!!!] ERROR <%s> ", str);
	}
	else if (level == YEET)
	{
		snprintf(format, 420, "{%d %llx} %s%s", smp_processor_id(), ((__u64)current) & 0xffff, "[<(* * <) <(* *)> (> * *)>] Y E E T <%s> ", str);
	}
	else
	{
		snprintf(format, 420, "{%d %04llx} %s%s", smp_processor_id(), ((__u64)current) & 0xffff, "[%s] ", str);
	}
	if (level == HUGE_ERROR)
	{
		printk(KERN_INFO "\n");
	}

	va_start(args, str);
	vsnprintf(log_message, 420, format, args);
	printk(KERN_INFO "%s", log_message);

	// TODO: Make this work. Specify log level somehow
	//vprintk(format, args);
}
