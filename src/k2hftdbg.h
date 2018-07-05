/*
 * k2hftfuse for file transaction by FUSE-based file system
 * 
 * Copyright 2015 Yahoo Japan Corporation.
 * 
 * k2hftfuse is file transaction system on FUSE file system with
 * K2HASH and K2HASH TRANSACTION PLUGIN, CHMPX.
 * 
 * For the full copyright and license information, please view
 * the license file that was distributed with this source code.
 *
 * AUTHOR:   Takeshi Nakatani
 * CREATE:   Wed Sep 2 2015
 * REVISION:
 *
 */

#ifndef	K2HFTDBG_H
#define K2HFTDBG_H

#include "k2hftcommon.h"

DECL_EXTERN_C_START

//---------------------------------------------------------
// Debug
//---------------------------------------------------------
typedef enum k2hft_dbg_mode{
	K2HFTDBG_SILENT	= 0,
	K2HFTDBG_ERR	= 1,
	K2HFTDBG_WARN	= 3,
	K2HFTDBG_MSG	= 7,
	K2HFTDBG_DUMP	= 15
}K2hFtDbgMode;

extern K2hFtDbgMode		k2hft_debug_mode;			// Do not use directly this variable.
extern FILE*			k2hft_dbg_fp;
extern bool				k2hft_foreground_mode;

void InitK2hFtDbgSyslog(void);
K2hFtDbgMode SetK2hFtDbgMode(K2hFtDbgMode mode);
bool SetK2hFtDbgModeByString(const char* strmode);
K2hFtDbgMode BumpupK2hFtDbgMode(void);
K2hFtDbgMode GetK2hFtDbgMode(void);
bool LoadK2hFtDbgEnv(void);
bool SetK2hFtDbgFile(const char* filepath);
bool UnsetK2hFtDbgFile(void);
bool SetK2hFtSignalUser1(void);

//---------------------------------------------------------
// Debugging Macros
//---------------------------------------------------------
#define	K2HFTDBGMODE_STR(mode)		(	K2HFTDBG_SILENT	== mode ? "SLT" : \
										K2HFTDBG_ERR	== mode ? "ERR" : \
										K2HFTDBG_WARN	== mode ? "WAN" : \
										K2HFTDBG_MSG	== mode ? "MSG" : \
										K2HFTDBG_DUMP	== mode ? "DMP" : "")

// Convert dbgmode to sysylog level(without SILENT)
#define	CVT_K2HFTDBGMODE_SYSLOGLEVEL(mode) \
		(	K2HFTDBG_MSG	== (mode & K2HFTDBG_MSG)	? LOG_INFO		: \
			K2HFTDBG_WARN	== (mode & K2HFTDBG_WARN)	? LOG_WARNING	: \
			K2HFTDBG_ERR	== (mode & K2HFTDBG_ERR)	? LOG_ERR		: LOG_CRIT	)

// syslog
#define	LOW_K2HFTSYSLOG(level, fmt, ...) \
		if(!k2hft_foreground_mode){ \
			syslog(level, fmt "%s", __VA_ARGS__); \
		}

// print
#define	LOW_K2HFTPRINT(mode, fmt, ...) \
		if(k2hft_foreground_mode && (k2hft_debug_mode & mode) == mode){ \
			fprintf((k2hft_dbg_fp ? k2hft_dbg_fp : stderr), "[%s] %s(%d) : " fmt "%s\n", K2HFTDBGMODE_STR(mode), __func__, __LINE__, __VA_ARGS__); \
		}

#define	K2HFTPRINT(mode, ...) \
		if(0 != (k2hft_debug_mode & mode)){ \
			LOW_K2HFTPRINT(mode, __VA_ARGS__); \
			LOW_K2HFTSYSLOG(CVT_K2HFTDBGMODE_SYSLOGLEVEL(mode), __VA_ARGS__); \
		}

#define	SLT_K2HFTPRN(...)			K2HFTPRINT(K2HFTDBG_SILENT,		##__VA_ARGS__, "")	// This means nothing...
#define	ERR_K2HFTPRN(...)			K2HFTPRINT(K2HFTDBG_ERR,		##__VA_ARGS__, "")
#define	WAN_K2HFTPRN(...)			K2HFTPRINT(K2HFTDBG_WARN,		##__VA_ARGS__, "")
#define	MSG_K2HFTPRN(...)			K2HFTPRINT(K2HFTDBG_MSG,		##__VA_ARGS__, "")
#define	FORCE_K2HFTPRN(...)			LOW_K2HFTPRINT(K2HFTDBG_SILENT,	##__VA_ARGS__, ""); \
									syslog(LOG_CRIT,				__VA_ARGS__);		// care for K2HFTDBG_SILENT

#define	IS_K2HFTDBG_DUMP()			(K2HFTDBG_DUMP == k2hft_debug_mode)

DECL_EXTERN_C_END

#endif	// K2HFTDBG_H

/*
 * VIM modelines
 *
 * vim:set ts=4 fenc=utf-8:
 */
