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
 * CREATE:   Thu Sep 17 2015
 * REVISION:
 *
 */

#ifndef	K2HFTSVRINFO_H
#define K2HFTSVRINFO_H

#include <yaml.h>
#include <string>
#include <vector>
#include <map>

#include "k2hftplugin.h"
#include "k2hftutil.h"
#include "k2hftdbg.h"

//---------------------------------------------------------
// Symbols & Macros
//---------------------------------------------------------
#define	K2HFTFSVR_FILE_TYPE				1
#define	K2HFTFSVR_TRANS_TYPE			2
#define	K2HFTFSVR_BOTH_TYPE				3

#define	IS_K2HFTFSVR_FILE_TYPE(type)	(K2HFTFSVR_FILE_TYPE	== (type & K2HFTFSVR_FILE_TYPE))
#define	IS_K2HFTFSVR_TRANS_TYPE(type)	(K2HFTFSVR_TRANS_TYPE	== (type & K2HFTFSVR_TRANS_TYPE))
#define	IS_K2HFTFSVR_BOTH_TYPE(type)	(K2HFTFSVR_BOTH_TYPE	== (type & K2HFTFSVR_BOTH_TYPE))

#define	IS_K2HFTFSVR_SAFE_TYPE(type)	(0 == (type & ~K2HFTFSVR_BOTH_TYPE))

#define	STR_K2HFTFSVR_TYPE(type)		(	K2HFTFSVR_BOTH_TYPE	== (type & K2HFTFSVR_BOTH_TYPE)	? "file/trans"	:			\
											K2HFTFSVR_FILE_TYPE	== (type & K2HFTFSVR_FILE_TYPE)	? "file"		:			\
											K2HFTFSVR_TRANS_TYPE== (type & K2HFTFSVR_TRANS_TYPE)? "trans"		: "unknown"	)

//---------------------------------------------------------
// Structure
//---------------------------------------------------------
//
// For ns print
//
typedef struct k2hftsvr_time_part{
	bool			is_type_ns;
	std::string		form;
}K2HFTSVRTIMEPART, *PK2HFTSVRTIMEPART;

typedef std::vector<K2HFTSVRTIMEPART>			k2hftsvrtplist_t;

//
// For all format
//
typedef enum k2hftsvr_format_part_type{
	K2HFTSVR_FPT_STRING,						// 
	K2HFTSVR_FPT_CONTENTS,						// %L
	K2HFTSVR_FPT_TIME,							// %T(%t)
	K2HFTSVR_FPT_FILENAME,						// %f
	K2HFTSVR_FPT_FILEPATH,						// %F
	K2HFTSVR_FPT_PID,							// %P(%p)
	K2HFTSVR_FPT_HOSTNAME,						// %H(%h)
	K2HFTSVR_FPT_TRIM_CONTENTS					// %l
}K2HFTSVRFPT;

typedef struct k2hftsvr_form_part{
	K2HFTSVRFPT		type;
	std::string		form;
}K2HFTSVRFORMPART, *PK2HFTSVRFORMPART;

typedef std::vector<K2HFTSVRFORMPART>			k2hftsvrfplist_t;

//
// For plugin
//
typedef std::map<std::string, PK2HFT_PLUGIN>	k2hftpluginpath_t;

//---------------------------------------------------------
// Class K2hFtSvrInfo
//---------------------------------------------------------
class K2hFtSvrInfo
{
	protected:
		std::string			Config;					// not allow empty
		uint64_t			OutputType;
		bool				IsBinaryMode;
		bool				IsNoConvert;
		std::string			OutputBaseDir;			// terminated by '/'
		std::string			UnifyFilePath;			// absolute file path(but relative path in INI)
		std::string			TransConfig;			// allow empty(because case of loading environments)
		std::string			DtorCtpPath;
		bool				IsTransK2hMemType;		// memory type k2hash
		std::string			TransK2hFilePath;		// not empty for only file type
		bool				IsTransK2hInit;			// can disable at only file type
		bool				IsTransK2hFullmap;		// can disable at only file type
		int					TransK2hMaskBitCnt;
		int					TransK2hCMaskBitCnt;
		int					TransK2hEleCnt;
		size_t				TransK2hPageSize;
		int					TransK2hThreadCnt;
		k2hftsvrtplist_t	TimeFormat;
		k2hftsvrfplist_t	OutputFormat;
		k2hftpluginpath_t	pluginpathmap;			// plugin path mapping

	protected:
		void Clean(void);

		bool LoadIni(const char* conffile, K2hFtPluginMan& pluginman);
		bool LoadYaml(const char* config, K2hFtPluginMan& pluginman, bool is_json_string);
		bool LoadYamlTopLevel(yaml_parser_t& yparser, K2hFtPluginMan& pluginman);
		bool LoadYamlMainSec(yaml_parser_t& yparser, K2hFtPluginMan& pluginman);

	public:
		K2hFtSvrInfo(const char* path = NULL, K2hFtPluginMan* ppluginman = NULL);
		virtual ~K2hFtSvrInfo(void);

		inline bool IsLoad(void) const { return !Config.empty(); }
		inline const char* GetConfiguration(void) const { return Config.c_str(); }
		inline bool IsOutputFileMode(void) const { return IS_K2HFTFSVR_FILE_TYPE(OutputType); }
		inline bool IsTransMode(void) const { return IS_K2HFTFSVR_TRANS_TYPE(OutputType); }
		inline bool IsBinMode(void) const { return IsBinaryMode; }
		inline bool IsUnityFileMode(void) const { return !UnifyFilePath.empty(); }
		inline bool IsNeedConvert(void) const { return !IsNoConvert; }
		inline const char* GetOutputBaseDir(void) const { return OutputBaseDir.c_str(); }
		inline const char* GetUnityFilePath(void) const { return UnifyFilePath.c_str(); }

		inline const char* GetConfigTransK2h(void) const { return TransConfig.c_str(); }
		inline const char* GetCtpDtorPath(void) const { return DtorCtpPath.c_str(); }
		inline bool IsMemoryTypeTransK2h(void) const { return IsTransK2hMemType; }
		inline const char* GetPathTransK2h(void) const { return TransK2hFilePath.c_str(); }
		inline bool IsInitializeTransK2h(void) const { return IsTransK2hInit; }
		inline bool IsFullmapTransK2h(void) const { return IsTransK2hFullmap; }
		inline int GetMaskTransK2h(void) const { return TransK2hMaskBitCnt; }
		inline int GetCMaskTransK2h(void) const { return TransK2hCMaskBitCnt; }
		inline int GetElementCntTransK2h(void) const { return TransK2hEleCnt; }
		inline size_t GetPageSizeTransK2h(void) const { return TransK2hPageSize; }
		inline int GetThreadCntTransK2h(void) const { return TransK2hThreadCnt; }

		bool Load(const char* config, K2hFtPluginMan& pluginman);
		bool Dump(void) const;

		bool ConvertOutput(std::string& stroutput, const char* hostname, pid_t pid, const char* filepath, const struct timespec* ts, const char* content) const;

		bool IsPluginEmpty(void) const { return pluginpathmap.empty(); }
		PK2HFT_PLUGIN FindPlugin(std::string& outputpath);
		PK2HFT_PLUGIN GetPlugin(std::string& outputpath, std::string& basedir, K2hFtPluginMan& pluginman);
};

#endif	// K2HFTSVRINFO_H

/*
 * VIM modelines
 *
 * vim:set ts=4 fenc=utf-8:
 */
