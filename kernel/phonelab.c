#include <linux/types.h>
#include <linux/slab.h>
#include <linux/printk.h>

#include <linux/phonelab.h>

#define MAX_LOG_LEN	(4 * 1024)
static void alog(LOGCAT_LEVEL level, char *tag, const char *fmt, va_list args)
{
	char *buffer = kmalloc(sizeof(char) * MAX_LOG_LEN, GFP_KERNEL);

	if (buffer == NULL)
	{
		printk(KERN_DEBUG "alog buffer allocation failed\n");
		return;
	}

	memset(buffer, 0, MAX_LOG_LEN);
	vsnprintf(buffer, MAX_LOG_LEN, fmt, args);

	printk(KERN_INFO "%s|%d|%s|%s\n", PHONELAB_MAGIC, level, tag, buffer);

	kfree(buffer);
}

void alog_v(char *tag, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	alog(ANDROID_LOG_VERBOSE, tag, fmt, args);
	va_end(args);
}

void alog_d(char *tag, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	alog(ANDROID_LOG_DEBUG, tag, fmt, args);
}

void alog_i(char *tag, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	alog(ANDROID_LOG_INFO, tag, fmt, args);
	va_end(args);
}

void alog_w(char *tag, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	alog(ANDROID_LOG_WARN, tag, fmt, args);
	va_end(args);
}

void alog_e(char *tag, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	alog(ANDROID_LOG_ERROR, tag, fmt, args);
	va_end(args);
}

