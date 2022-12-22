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
 * CREATE:   Fri Sep 4 2015
 * REVISION:
 *
 */

#ifndef	K2HFTINFO_H
#define K2HFTINFO_H

#include <regex.h>
#include <yaml.h>
#include <k2hash/k2hshm.h>

#include <string>
#include <vector>
#include <map>

#include "k2hftplugin.h"
#include "k2hftfdcache.h"
#include "k2hftstructure.h"
#include "k2hftutil.h"
#include "k2hftdbg.h"

//---------------------------------------------------------
// Symbols
//---------------------------------------------------------
#define	REGEX_ERROR_BUFF_SIZE			1024

//---------------------------------------------------------
// Structure
//---------------------------------------------------------
//
// name and small stat for directory listing
//
typedef struct k2hft_list_dir{
	std::string		strname;
	struct stat*	pstat;

	k2hft_list_dir(void) : strname(""), pstat(NULL) {}
	~k2hft_list_dir(void)
	{
		K2HFT_DELETE(pstat);
	}
}K2HFTLISTDIR, *PK2HFTLISTDIR;

typedef std::vector<PK2HFTLISTDIR>			k2hftldlist_t;

inline void free_k2hftldlist(k2hftldlist_t& list)
{
	for(k2hftldlist_t::iterator iter = list.begin(); iter != list.end(); ++iter){
		K2HFT_DELETE(*iter);
	}
	list.clear();
}

//
// replacing pattern structure for Regex
//
typedef struct k2hft_replace_pattern{
	int					matchno;			// -1 means static string, 0 is all, 1... is match position.
	std::string			substring;

	k2hft_replace_pattern(void) : matchno(-1), substring("") {}
	~k2hft_replace_pattern(void) {}
}K2HFTREPPTN, *PK2HFTREPPTN;

typedef std::vector<PK2HFTREPPTN>			k2hftreplist_t;

inline void free_replace_pattern_list(k2hftreplist_t& list)
{
	for(k2hftreplist_t::iterator iter = list.begin(); iter != list.end(); ++iter){
		K2HFT_DELETE(*iter);
	}
	list.clear();
}

inline void duplicate_replace_pattern_list(const k2hftreplist_t& src, k2hftreplist_t& dst)
{
	for(k2hftreplist_t::const_iterator iter = src.begin(); iter != src.end(); ++iter){
		PK2HFTREPPTN	ptmp= new K2HFTREPPTN;
		ptmp->matchno		= (*iter)->matchno;
		ptmp->substring		= (*iter)->substring;
		dst.push_back(ptmp);
	}
}

//
// compare pattern structure for Regex and Static string matching
//
typedef struct k2hft_match_pattern{
	bool				IsRegex;
	std::string			BaseString;
	regex_t				RegexBuffer;
	k2hftreplist_t		ReplaceList;		// if regex but this list is empty, so all of line should be transferred.

	k2hft_match_pattern(void) : IsRegex(false), BaseString("") {}
	~k2hft_match_pattern(void)
	{
		if(IsRegex){
			regfree(&RegexBuffer);
		}
		free_replace_pattern_list(ReplaceList);
	}
}K2HFTMATCHPTN, *PK2HFTMATCHPTN;

typedef std::vector<PK2HFTMATCHPTN>			k2hftmatchlist_t;

inline void free_match_list(k2hftmatchlist_t& list)
{
	for(k2hftmatchlist_t::iterator iter = list.begin(); iter != list.end(); ++iter){
		K2HFT_DELETE(*iter);
	}
	list.clear();
}

inline bool duplicate_match_list(const k2hftmatchlist_t& src, k2hftmatchlist_t& dst)
{
	for(k2hftmatchlist_t::const_iterator iter = src.begin(); iter != src.end(); ++iter){
		PK2HFTMATCHPTN	ptmp= new K2HFTMATCHPTN;
		ptmp->IsRegex		= (*iter)->IsRegex;
		ptmp->BaseString	= (*iter)->BaseString;

		if(ptmp->IsRegex){
			// compile regex
			int		regresult;
			if(0 != (regresult = regcomp(&(ptmp->RegexBuffer), ptmp->BaseString.c_str(), REG_EXTENDED | REG_NEWLINE))){
				char	errbuf[REGEX_ERROR_BUFF_SIZE];
				errbuf[0] = '\0';
				regerror(regresult, &(ptmp->RegexBuffer), errbuf, REGEX_ERROR_BUFF_SIZE);

				ERR_K2HFTPRN("Failed to compile compptn(%s) regex, error is %s(%d)", ptmp->BaseString.c_str(), errbuf, regresult);
				K2HFT_DELETE(ptmp);
				return false;
			}
		}
		duplicate_replace_pattern_list((*iter)->ReplaceList, ptmp->ReplaceList);

		dst.push_back(ptmp);
	}
	return true;
}

//
// rule structure
//
typedef struct k2hft_rule{
	std::string			TargetPath;			// not empty
	bool				IsTransfer;
	std::string			OutputPath;			// If TargetPath is dir, on this case value allow directory terminated '/'. (if file, value is always file path)
	PK2HFT_PLUGIN		pPlugin;			// plugin if specified
	bool				DefaultDenyAll;		// true means "from deny all to allow"
	mode_t				Mode;
	uid_t				Uid;
	gid_t				Gid;
	off_t				AccumSize;			// accumulate transferred data size
	struct timespec		StartTime;			// start time of up
	struct timespec		LastTime;			// last time of transferring
	k2hftmatchlist_t	MatchingList;		// matching pattern list

	k2hft_rule(void) :	TargetPath(""), IsTransfer(false), OutputPath(""), pPlugin(NULL), DefaultDenyAll(true),
						Mode(0), Uid(0), Gid(0), AccumSize(0), StartTime(common_init_time), LastTime(common_init_time)
	{
	}
	~k2hft_rule(void)
	{
		K2HFT_DELETE(pPlugin);
		free_match_list(MatchingList);
	}
}K2HFTRULE, *PK2HFTRULE;

typedef	std::map<std::string, PK2HFTRULE>	k2hftrulemap_t;

//---------------------------------------------------------
// Class K2hFtInfo
//---------------------------------------------------------
class K2hFtManage;

// cppcheck-suppress noCopyConstructor
class K2hFtInfo
{
	friend class K2hFtManage;

	protected:
		int*			prule_lockval;		// like mutex for variables
		std::string		Config;
		bool			IsMemType;			// memory type k2hash
		std::string		K2hFilePath;		// not empty for only file type
		bool			IsInitialize;		// can disable at only file type
		bool			IsFullmap;			// can disable at only file type
		int				MaskBitCnt;
		int				CMaskBitCnt;
		int				MaxElementCnt;
		size_t			PageSize;
		int				DtorThreadCnt;
		std::string		DtorCtpPath;
		bool			IsBinaryMode;
		time_t			ExpireTime;
		mode_t			DirMode;
		uid_t			DirUid;
		gid_t			DirGid;
		struct timespec	InitTime;
		k2hftrulemap_t	RuleMap;

	protected:
		void Clean(void);
		bool InitializeOutputFiles(void) const;
		bool CheckBinaryMode(void);

		bool LoadIni(const char* conffile, mode_t init_mode, mode_t init_dmode, uid_t init_uid, gid_t init_gid, K2hFtPluginMan& pluginman);
		bool LoadYaml(const char* config, mode_t init_mode, mode_t init_dmode, uid_t init_uid, gid_t init_gid, K2hFtPluginMan& pluginman, bool is_json_string);
		bool LoadYamlTopLevel(yaml_parser_t& yparser, mode_t init_mode, mode_t init_dmode, uid_t init_uid, gid_t init_gid, K2hFtPluginMan& pluginman);
		bool LoadYamlMainSec(yaml_parser_t& yparser, mode_t init_mode, mode_t init_dmode, uid_t init_uid, gid_t init_gid, K2hFtPluginMan& pluginman);
		bool LoadYamlRuleSec(yaml_parser_t& yparser, mode_t init_mode, mode_t init_dmode, uid_t init_uid, gid_t init_gid, K2hFtPluginMan& pluginman, bool is_dir_rule);
		bool LoadYamlAclPart(yaml_parser_t& yparser, k2hftmatchlist_t& Matchs);
		bool LoadYamlAclOneValue(yaml_parser_t& yparser, k2hftmatchlist_t& Matchs);

	public:
		explicit K2hFtInfo(const char* conffile = NULL, mode_t init_mode = 0, mode_t init_dmode = 0, uid_t init_uid = 0, gid_t init_gid = 0, K2hFtPluginMan* ppluginman = NULL);
		virtual ~K2hFtInfo(void);

		inline bool IsLoad(void) const { return !Config.empty(); }
		inline bool IsBinMode(void) const { return IsBinaryMode; }
		inline const char* GetCtpDtorPath(void) const { return DtorCtpPath.c_str(); }
		inline const time_t* GetExpireTime(void) const { return (ExpireTime <= 0 ? NULL : &ExpireTime); }
		bool Load(const char* config, mode_t init_mode, mode_t init_dmode, uid_t init_uid, gid_t init_gid, K2hFtPluginMan& pluginman);
		bool Dump(void) const;

		bool FindPath(const char* path, struct stat& stbuf) const;
		uint64_t GetFileHandle(const char* path) const;

		bool ReadDir(const char* path, k2hftldlist_t& list) const;
		bool TruncateZero(const char* path);
		bool SetOwner(const char* path, uid_t uid, gid_t gid);
		bool SetMode(const char* path, mode_t mode);
		bool SetTimespec(const char* path, const struct timespec& ts);

		bool CreateFile(const char* path, mode_t mode, uid_t uid, gid_t gid, uint64_t& handle, K2hFtPluginMan& pluginman);
		bool UpdateMTime(uint64_t handle);

		unsigned char* CvtPushData(const unsigned char* data, size_t length, uint64_t filehandle, size_t& cvtlength) const;
		bool Processing(K2HShm* pk2hash, PK2HFTVALUE pValue, uint64_t filehandle, K2hFtFdCache& fdcache, K2hFtPluginMan& pluginman);
};

#endif	// K2HFTINFO_H

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noexpandtab sw=4 ts=4 fdm=marker
 * vim<600: noexpandtab sw=4 ts=4
 */
