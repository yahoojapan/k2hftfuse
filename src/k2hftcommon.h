/*
 * k2hftfuse for file transaction by FUSE-based file system
 * 
 * Copyright 2015 Yahoo! JAPAN corporation.
 * 
 * k2hftfuse is file transaction system on FUSE file system with
 * K2HASH and K2HASH TRANSACTION PLUGIN, CHMPX.
 * 
 * For the full copyright and license information, please view
 * the LICENSE file that was distributed with this source code.
 *
 * AUTHOR:   Takeshi Nakatani
 * AUTHOR:   nakatani@yahoo-corp.jp
 * CREATE:   Wed Sep 2 2015
 * REVISION:
 *
 */

#ifndef	K2HFTCOMMON_H
#define K2HFTCOMMON_H

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdarg.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

//---------------------------------------------------------
// Macros for compiler
//---------------------------------------------------------
#ifndef	K2HFT_NOPADDING
#define	K2HFT_ATTR_PACKED			__attribute__ ((packed))
#else	// K2HFT_NOPADDING
#define	K2HFT_ATTR_PACKED
#endif	// K2HFT_NOPADDING

#if defined(__cplusplus)
#define	DECL_EXTERN_C_START				extern "C" {
#define	DECL_EXTERN_C_END				}
#else	// __cplusplus
#define	DECL_EXTERN_C_START
#define	DECL_EXTERN_C_END
#endif	// __cplusplus

//---------------------------------------------------------
// Symbols
//---------------------------------------------------------
#define	K2HFT_INVALID_HANDLE			(-1)
#define	K2HFT_INVALID_PID				(-1)

// deafult dtor
#define	K2HFT_K2HTPDTOR					"libk2htpdtor.so"

//---------------------------------------------------------
// Templates
//---------------------------------------------------------
#if defined(__cplusplus)
	inline bool K2HFT_ISEMPTYSTR(const char* pstr)
	{
		return (NULL == (pstr) || '\0' == *(pstr)) ? true : false;
	}

	inline void K2HFTPRN(const char* format, ...)
	{
		if(format){
			va_list ap;
			va_start(ap, format);
			fprintf(stdout, "MESSAGE - ");
			vfprintf(stdout, format, ap); 
			va_end(ap);
		}
		fprintf(stdout, "\n");
	}

	inline void K2HFTERR(const char* format, ...)
	{
		if(format){
			va_list ap;
			va_start(ap, format);
			fprintf(stderr, "ERROR - ");
			vfprintf(stderr, format, ap); 
			va_end(ap);
		}
		fprintf(stderr, "\n");
	}

#else	// __cplusplus
	#define	K2HFT_ISEMPTYSTR(pstr)		(NULL == (pstr) || '\0' == *(pstr))

	#define K2HFTPRN(...)				fprintf(stdout, "MESSAGE - "); \
										fprintf(stdout, __VA_ARGS__); \
										fprintf(stdout, "\n");

	#define K2HFTERR(...)				fprintf(stderr, "ERROR - "); \
										fprintf(stderr, __VA_ARGS__); \
										fprintf(stderr, "\n");

#endif	// __cplusplus

#define	K2HFT_FREE(ptr)					if(ptr){ \
											free(ptr); \
											ptr = NULL; \
										}

#define	K2HFT_DELETE(ptr)				if(ptr){ \
											delete ptr; \
											ptr = NULL; \
										}

#define	K2HFT_CLOSE(fd)					if(K2HFT_INVALID_HANDLE != fd){ \
											close(fd); \
											fd = K2HFT_INVALID_HANDLE; \
										}


#endif	// K2HFTCOMMON_H

/*
 * VIM modelines
 *
 * vim:set ts=4 fenc=utf-8:
 */
