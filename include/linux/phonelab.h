#ifndef __PHONELAB__H_
#define __PHONELAB__H_

#include <linux/types.h>

#define PHONELAB_MAGIC "<PhoneLab>"

/* This whole enum is borrowed from system/core/include/android/log.h */
typedef enum {
	ANDROID_LOG_UNKNOWN = 0,
	ANDROID_LOG_DEFAULT,    /* only for SetMinPriority() */
	ANDROID_LOG_VERBOSE,
	ANDROID_LOG_DEBUG,
	ANDROID_LOG_INFO,
	ANDROID_LOG_WARN,
	ANDROID_LOG_ERROR,
	ANDROID_LOG_FATAL,
	ANDROID_LOG_SILENT,     /* only for SetMinPriority(); must be last */
} LOGCAT_LEVEL;

void alog_v(char *tag, char *payload);
void alog_d(char *tag, char *payload);
void alog_i(char *tag, char *payload);
void alog_w(char *tag, char *payload);
void alog_e(char *tag, char *payload);

#endif	/* __PHONELAB__H_ */
