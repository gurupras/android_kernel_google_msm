#include <linux/types.h>
#include <linux/slab.h>
#include <linux/printk.h>

#include <linux/phonelab.h>

static void alog(LOGCAT_LEVEL level, char *tag, char *payload)
{
	printk(KERN_INFO "%s|%d|%s|%s\n",
			PHONELAB_MAGIC,
			level,
			tag,
			payload);
}

inline void alog_v(char *tag, char *payload)
{
	alog(ANDROID_LOG_VERBOSE, tag, payload);
}

inline void alog_d(char *tag, char *payload)
{
	alog(ANDROID_LOG_DEBUG, tag, payload);
}

inline void alog_i(char *tag, char *payload)
{
	alog(ANDROID_LOG_INFO, tag, payload);
}

inline void alog_w(char *tag, char *payload)
{
	alog(ANDROID_LOG_WARN, tag, payload);
}

inline void alog_e(char *tag, char *payload)
{
	alog(ANDROID_LOG_ERROR, tag, payload);
}

