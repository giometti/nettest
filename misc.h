/*
 * Copyright (C) 2022   Rodolfo Giometti <giometti@enneenne.com>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef _MISC_H
#define _MISC_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>

/*
 * Misc definitions & macros
 */

#define NAME			program_invocation_short_name

#define fallthrough		do {} while (0)
#define likely(x)               __builtin_expect(!!(x), 1)
#define unlikely(x)             __builtin_expect(!!(x), 0)
#define __deprecated            __attribute__ ((deprecated))
#define __packed                __attribute__ ((packed))
#define __constructor           __attribute__ ((constructor))
#define stringify(s)		__stringify(s)
#define __stringify(s)		#s

#define BUILD_BUG_ON(c, msg)            _Static_assert (c, msg);
#define BUILD_BUG_ON_ZERO(e)            (sizeof(char[1 - 2 * !!(e)]) - 1)
#define __must_be_array(a)                                              \
                BUILD_BUG_ON_ZERO(__builtin_types_compatible_p(typeof(a), \
                                                        typeof(&a[0])))
#define ARRAY_SIZE(arr)                                                 \
                (sizeof(arr) / sizeof((arr)[0]) + __must_be_array(arr))
#define membersizeof(type, member)					\
				sizeof(((type *)0)->member)

#define min(a,b) ({ \
	__typeof__ (a) _a = (a); \
	__typeof__ (b) _b = (b); \
	_a < _b ? _a : _b;\
})

#define max(a,b) ({ \
	__typeof__ (a) _a = (a); \
	__typeof__ (b) _b = (b); \
	_a > _b ? _a : _b;\
})

extern int nettest_debug_level;
extern int nettest_add_time;
#define __message(stream, layout, fmt, args...)				\
        do {                                                            \
		struct timespec t;					\
		if (nettest_add_time)					\
			clock_gettime(CLOCK_MONOTONIC, &t);		\
                switch (layout) {					\
		case 0:							\
                        fprintf(stream, "[%s] " fmt "\n", NAME, ## args);\
                        break;                                          \
		case 1:							\
                        if (unlikely(nettest_debug_level >= layout)) {	\
				if (nettest_add_time)			\
					fprintf(stream, "%ld.%09ld ",	\
						t.tv_sec, t.tv_nsec);	\
				fprintf(stream, "[%s] %s: " fmt "\n", 	\
					NAME, __func__, ## args);	\
			}						\
                        break;                                          \
		default:						\
                        if (unlikely(nettest_debug_level >= layout)) {	\
				if (nettest_add_time)			\
					fprintf(stream, "%ld.%09ld ",	\
						t.tv_sec, t.tv_nsec);	\
                                fprintf(stream, "[%s](%s@%d) %s: " fmt "\n",\
                                        NAME, __FILE__, __LINE__, __func__, ## args);\
			}						\
                }                                                       \
        } while (0)

#define alert(fmt, args...)                                             \
                __message(stderr, 0, fmt, ## args)
#define err(fmt, args...)                                               \
                __message(stderr, 0, fmt, ## args)
#define err_if(exp, fmt, args...)					\
		do {							\
			if (unlikely(exp))				\
				err("(%s) " fmt , #exp , ## args);	\
		} while (0)
#define err_if_exit(exp, ret, fmt, args...)				\
		do {							\
			if (unlikely(exp)) {				\
				err("(%s) " fmt , #exp , ## args);	\
				exit(ret);				\
			}						\
		} while (0)
#define err_if_ret(exp, ret, fmt, args...)				\
		do {							\
			if (unlikely(exp)) {				\
				err("(%s) " fmt , #exp , ## args);	\
				return ret;				\
			}						\
		} while (0)
#define warn(fmt, args...)                                              \
                __message(stderr, 0, fmt , ## args)
#define warn_if(exp, fmt, args...)					\
		do {							\
			if (unlikely(exp))				\
				warn("(%s) " fmt , #exp , ## args);	\
		} while (0)
#define info(fmt, args...)                                              \
                __message(stderr, 0, fmt, ## args)
#define dbg(fmt, args...)                                               \
                __message(stderr, 1, fmt, ## args)
#define dbg_if(exp, fmt, args...)					\
		do {							\
			if (unlikely(exp)) {				\
				dbg("(%s) " fmt , #exp , ## args);	\
			}						\
		} while (0)
#define dbg_if_ret(exp, ret, fmt, args...)				\
		do {							\
			if (unlikely(exp)) {				\
				dbg("(%s) " fmt , #exp , ## args);	\
				return ret;				\
			}						\
		} while (0)
#define vdbg(fmt, args...)						\
                __message(stderr, 3, fmt, ## args)
#define vdbg_if_ret(exp, ret, fmt, args...)				\
		do {							\
			if (unlikely(exp)) {				\
				vdbg("(%s) " fmt , #exp , ## args);	\
				return ret;				\
			}						\
		} while (0)

#define dump(buf, len, fmt, args...)					\
        do {                                                            \
                size_t i;						\
                fprintf(stderr, "%s[%4d] %s: " fmt "\n" ,               \
                        __FILE__, __LINE__, __func__ , ## args);        \
		if (likely((len) > 0)) {				\
			fprintf(stderr, "          00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f\n  0x0000: "); \
			for (i = 0; i < (len); i++) {			\
				fprintf(stderr, "%02x ", buf[i]);              \
				if ((i + 1) % 16 == 0)                         \
					fprintf(stderr, "\n  0x%04zx: ", i + 1);\
			}						\
			fprintf(stderr, "\n");				\
		}							\
        } while (0)

#define dbg_dump(buf, len, fmt, args...)				\
        do {                                                            \
                if (unlikely(nettest_debug_level == 2))			\
			dump(buf, len, fmt, ## args);			\
        } while (0)

#ifdef HAVE_EXECINFO_H
#define stack_trace()                                                   \
        do {                                                            \
                void *a[10];                                            \
                size_t size;                                            \
                char **str;                                             \
                size_t i;                                               \
                                                                        \
                size = backtrace(a, 10);                                \
                str = backtrace_symbols(a, size);                       \
                                                                        \
                if (size > 0) {                                         \
                        err("back trace:");                             \
                        for (i = 0; i < size; i++)                      \
                                err("%s", str[i]);                      \
                }                                                       \
                                                                        \
                free(str);                                              \
        } while (0)
#else
#define stack_trace()                                                   \
        do {                                                            \
                /* nop */;                                              \
        } while (0)
#endif /* HAVE_EXECINFO_H */

#define if_ret(exp, ret)						\
		do {							\
			if (unlikely(exp)) 				\
				return ret;				\
		} while (0)

#define __CHECK(condition, do_exit)					\
        do {                                                            \
                err("fatal error in %s(): %s",				\
			__func__, stringify(condition));		\
                stack_trace();                                          \
		if (do_exit)						\
			exit(EXIT_FAILURE);				\
        } while (0)
#define __CHECK_ON(condition, do_exit)					\
        do {                                                            \
                if (unlikely(condition))                                \
                        __CHECK(condition, do_exit);			\
        } while(0)
#define BUG()								\
	__CHECK(offending line is __LINE__, 1)
#define BUG_ON(condition)						\
	__CHECK_ON(condition, 1)
#define WARN()								\
	__CHECK(offending line is __LINE__, 0)
#define WARN_ON(condition)						\
	__CHECK_ON(condition, 0)
#define EXIT_ON(condition)	BUG_ON(condition)

#ifndef PAGE_SIZE
#  define PAGE_SIZE             4096
#endif

#endif /* _MISC_H */