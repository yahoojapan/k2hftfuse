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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <fullock/flckstructure.h>
#include <fullock/flckbaselist.tcc>
#include <chmpx/chmconfutil.h>

#include "k2hftcommon.h"
#include "k2hftinfo.h"
#include "k2hftstructure.h"
#include "k2hftman.h"
#include "k2hftiniparser.h"
#include "k2hftwbuf.h"
#include "k2hftutil.h"
#include "k2hftdbg.h"

using namespace std;

//---------------------------------------------------------
// Symbols
//---------------------------------------------------------
// for regex
#define	K2HTFT_MAX_REG_MATCH			(INI_K2HFT_MAX_REPLACE_NUMBER + 1)

//---------------------------------------------------------
// Utility for parser
//---------------------------------------------------------
//
// Make K2HFTMATCHPTN structure from compare pattern and replace pattern string.
//
static PK2HFTMATCHPTN build_k2hft_match_pattern(const char* compptn, const char* replaceptn)
{
	if(K2HFT_ISEMPTYSTR(compptn)){
		ERR_K2HFTPRN("compptn is empty.");
		return NULL;
	}

	PK2HFTMATCHPTN	pMatchPtn = new K2HFTMATCHPTN;

	// check regex
	if('\"' != compptn[0]){
		//
		// not REGEX pattern
		//
		pMatchPtn->IsRegex 		= false;
		pMatchPtn->BaseString	= compptn;

	}else{
		//
		// REGEX pattern
		//
		if(strlen(compptn) < 3 || '\"' != compptn[strlen(compptn) - 1]){
			// compptn start '"', but it does not terminate character(\").
			ERR_K2HFTPRN("compptn(%s) does not terminate '\"' character.", compptn);
			K2HFT_DELETE(pMatchPtn);
			return NULL;
		}

		//
		// check & compile compare pattern
		//
		char*	ptmpcompptn = strdup(&compptn[1]);
		if(!ptmpcompptn){
			ERR_K2HFTPRN("Could not allocate memory.");
			K2HFT_DELETE(pMatchPtn);
			return NULL;
		}
		ptmpcompptn[strlen(ptmpcompptn) - 1] = '\0';

		// compile
		int		regresult;
		if(0 != (regresult = regcomp(&(pMatchPtn->RegexBuffer), ptmpcompptn, REG_EXTENDED | REG_NEWLINE))){
			char	errbuf[REGEX_ERROR_BUFF_SIZE];
			errbuf[0] = '\0';
			regerror(regresult, &(pMatchPtn->RegexBuffer), errbuf, REGEX_ERROR_BUFF_SIZE);

			ERR_K2HFTPRN("Failed to compile compptn(%s) regex, error is %s(%d)", ptmpcompptn, errbuf, regresult);
			K2HFT_FREE(ptmpcompptn);
			K2HFT_DELETE(pMatchPtn);
			return NULL;
		}

		// set base string without '"'
		pMatchPtn->BaseString = ptmpcompptn;
		K2HFT_FREE(ptmpcompptn);

		//
		// [CAREFUL] set regex flag HERE!
		//
		pMatchPtn->IsRegex = true;

		//
		// check replace pattern
		//
		if(K2HFT_ISEMPTYSTR(replaceptn)){
			// replace pattern is empty, it means all.
			//
			PK2HFTREPPTN	pRepPtn = new K2HFTREPPTN;
			pRepPtn->matchno		= 0;					// type is all($0)
			pRepPtn->substring		= "";
			pMatchPtn->ReplaceList.push_back(pRepPtn);

		}else if('\"' == replaceptn[0]){
			// Parse replace pattern.
			//
			char*	ptmprepptn = strdup(replaceptn);
			if(!ptmprepptn){
				ERR_K2HFTPRN("Could not allocate memory.");
				K2HFT_DELETE(pMatchPtn);
				return NULL;
			}

			char*	psetpos			= ptmprepptn;
			bool	is_last_escape	= false;
			bool	is_found_term	= false;
			for(char* preadpos = &ptmprepptn[1]; preadpos && '\0' != *preadpos; ++preadpos){
				if('\\' == *preadpos){
					// found escape character
					if(is_last_escape){
						*psetpos++		= *preadpos;					// '\\' -> '\'
						is_last_escape	= false;
					}else{
						is_last_escape	= true;
					}

				}else if('\"' == *preadpos){
					// found '"'
					if(is_last_escape){
						*psetpos++		= *preadpos;					// '\"' -> '"'
						is_last_escape	= false;
					}else{
						// not escape '"' is found, it is only allowed lastest word pos.
						if('\0' != preadpos[1]){
							// it is not end of string, so ERROR
							ERR_K2HFTPRN("replaceptn(%s) is regex, but there is no terminate character(\").", replaceptn);
							K2HFT_FREE(ptmprepptn);
							K2HFT_DELETE(pMatchPtn);
							return NULL;
						}

						// OK, it is regex terminated character(").
						*psetpos = '\0';								// terminate

						// add lastest static pattern into list
						if(0 < strlen(ptmprepptn)){
							PK2HFTREPPTN	pRepPtn = new K2HFTREPPTN;
							pRepPtn->matchno		= -1;				// type is static string
							pRepPtn->substring		= ptmprepptn;
							pMatchPtn->ReplaceList.push_back(pRepPtn);
						}

						// break loop without error.
						is_found_term = true;
						break;
					}

				}else if('&' == *preadpos){
					// found '&', '&' without escaping means replacing word for all.
					if(is_last_escape){
						*psetpos++				= *preadpos;			// '\&' -> '&'
						is_last_escape			= false;
					}else{
						// found key word('&' is as same as '$0')
						*psetpos				= '\0';					// terminate

						// add static pattern into list
						PK2HFTREPPTN	pRepPtn;
						if(0 < strlen(ptmprepptn)){
							pRepPtn 			= new K2HFTREPPTN;
							pRepPtn->matchno	= -1;					// type is static string
							pRepPtn->substring	= ptmprepptn;
							pMatchPtn->ReplaceList.push_back(pRepPtn);
						}

						// add replace pattern into list
						pRepPtn					= new K2HFTREPPTN;
						pRepPtn->matchno		= 0;					// type is all($0)
						pRepPtn->substring		= "";
						pMatchPtn->ReplaceList.push_back(pRepPtn);

						// reset set pos
						psetpos					= ptmprepptn;			// reset set pos
					}

				}else if('$' == *preadpos){
					// found '$', '$' without escaping and with number means replacing word for matching group.
					if(is_last_escape){
						*psetpos++		= *preadpos;					// '\$' -> '$'
						is_last_escape	= false;
					}else{
						// checking number key word after '$'
						int	number = -1;
						for(; '\0' != preadpos[1]; ++preadpos){
							if(0 == isdigit(preadpos[1])){
								break;
							}
							if(-1 == number){
								number = 0;
							}
							number = (number * 10) + atoi(&preadpos[1]);
						}
						if(-1 == number){
							// not found number word after '$'
							ERR_K2HFTPRN("replaceptn(%s) is regex, but it has '$' without number after it.", replaceptn);
							K2HFT_FREE(ptmprepptn);
							K2HFT_DELETE(pMatchPtn);
							return NULL;
						}else if(INI_K2HFT_MAX_REPLACE_NUMBER < number){
							// number is over INI_K2HFT_MAX_REPLACE_NUMBER
							ERR_K2HFTPRN("replaceptn(%s) is regex, but it has '$%d'. '$x' parameter must be under %d.", replaceptn, number, (INI_K2HFT_MAX_REPLACE_NUMBER + 1));
							K2HFT_FREE(ptmprepptn);
							K2HFT_DELETE(pMatchPtn);
							return NULL;
						}

						// found key word($x)
						*psetpos				= '\0';					// terminate

						// add static pattern into list
						PK2HFTREPPTN	pRepPtn;
						if(0 < strlen(ptmprepptn)){
							pRepPtn 			= new K2HFTREPPTN;
							pRepPtn->matchno	= -1;					// type is static string
							pRepPtn->substring	= ptmprepptn;
							pMatchPtn->ReplaceList.push_back(pRepPtn);
						}

						// add replace pattern into list
						pRepPtn					= new K2HFTREPPTN;
						pRepPtn->matchno		= number;				// type is $number($0...$9)
						pRepPtn->substring		= "";
						pMatchPtn->ReplaceList.push_back(pRepPtn);

						// reset set pos
						psetpos					= ptmprepptn;			// reset set pos
					}

				}else{
					// normal character
					if(is_last_escape){
						WAN_K2HFTPRN("replaceptn(%s) has single \"\\\" word, so it is skipped.", replaceptn);
						is_last_escape	= false;						// skip single '\' word
					}
					*psetpos++			= *preadpos;					// set normal character
				}
			}
			K2HFT_FREE(ptmprepptn);

			if(!is_found_term){
				// not found terminate character (")
				ERR_K2HFTPRN("replaceptn(%s) is regex, but there is no terminate character(\").", replaceptn);
				K2HFT_DELETE(pMatchPtn);
				return NULL;
			}

		}else{
			// compptn start '"' character(it means regex), but replaceptn does not start '"'.
			ERR_K2HFTPRN("replaceptn(%s) does not start '\"' character.", replaceptn);
			K2HFT_DELETE(pMatchPtn);
			return NULL;
		}
	}
	return pMatchPtn;
}

//---------------------------------------------------------
// Utility for regex
//---------------------------------------------------------
//
// Param:	tgdata			pointer for setting data
// 			offset			start of setting at tgdata, and update next position after setting
// 			tglength		length of tgdata, and update result data length(if reallocate in method, lenghth is changed.)
// 			data			base data
// 			length			length of base data
// 
static char* append_lastword(char* tgdata, size_t& tgoffset, size_t& tglength, const char* data, size_t length)
{
	// make last word
	string	lastwords;

	// check data last word
	size_t	for_del_last_pos;
	for(for_del_last_pos = 1; for_del_last_pos <= length && '\0' == data[length - for_del_last_pos]; ++for_del_last_pos);

	if(for_del_last_pos <= length){
		if('\n' == data[length - for_del_last_pos]){
			// '\n'
			lastwords = '\n';
		}else if('\r' == data[length - for_del_last_pos]){
			if(2 < length && '\n' == data[length - for_del_last_pos - 1]){
				// '\r\n'
				lastwords = "\r\n";
			}else{
				// '\r'
				lastwords = '\r';
			}
		}else{
			// no CR/LF -> '\n'
			lastwords = '\n';
		}
	}else{
		// no CR/LF -> '\n'
		lastwords = '\n';
	}

	// check tgdata last word position
	for(for_del_last_pos = 1; for_del_last_pos <= tgoffset && '\0' == data[tgoffset - for_del_last_pos]; ++for_del_last_pos);
	if(tgoffset < for_del_last_pos){
		tgoffset = 0;
	}else{
		tgoffset -= (for_del_last_pos - 1);
	}

	// check target buffer size
	if(tglength < (tgoffset + lastwords.length() + 1)){		// + length(\0)
		// need more tgdata area
		if(NULL == (tgdata = reinterpret_cast<char*>(realloc(tgdata, tgoffset + lastwords.length() + 1)))){
			ERR_K2HFTPRN("Could not allocate memory.");
			return NULL;
		}
		tglength = tgoffset + lastwords.length() + 1;
	}

	// append
	memcpy(&tgdata[tgoffset], lastwords.c_str(), lastwords.length());
	tgoffset			+= lastwords.length();
	tgdata[tgoffset++]	= '\0';								// + length(\0), tgoffset is next pos after '\0' char.

	return tgdata;
}

//
// Param:	tgdata			pointer for setting data
// 			offset			start of setting at tgdata, and update next position after setting
// 			tglength		length of tgdata, and update result data length(if reallocate in method, lenghth is changed.)
// 			data			base data
// 			length			length of base data
// 			match			position of base data for appending tgdata.
// 
static char* append_substring(char* tgdata, size_t& tgoffset, size_t& tglength, const char* data, size_t length, regmatch_t& match)
{
	if(-1 == match.rm_eo){
		// nothing to matching data, so nothing to do.
		return tgdata;
	}

	// check
	if(length < static_cast<size_t>(match.rm_so) || length < static_cast<size_t>(match.rm_eo)){
		// why?
		ERR_K2HFTPRN("Match data start position(%d) or end position(%d) is over data length(%zu).", match.rm_so, match.rm_eo, length);
		return NULL;
	}

	// check target buffer size
	size_t	cplength = static_cast<size_t>(match.rm_eo - match.rm_so);
	if(tglength < (tgoffset + cplength)){
		// need more tgdata area
		if(NULL == (tgdata = reinterpret_cast<char*>(realloc(tgdata, tgoffset + cplength)))){
			ERR_K2HFTPRN("Could not allocate memory.");
			return NULL;
		}
		tglength = tgoffset + cplength;
	}

	// append
	memcpy(&tgdata[tgoffset], &data[match.rm_so], cplength);
	tgoffset += cplength;

	return tgdata;
}

// [NOTE]
// pmatches array size is K2HTFT_MAX_REG_MATCH
//
static unsigned char* build_replace_result_data(k2hftreplist_t& replist, const unsigned char* data, size_t length, regmatch_t* pmatches, size_t& outputlen)
{
	outputlen = 0;

	// allocate
	size_t	rsize = length * 2;
	char*	presult;
	if(NULL == (presult = reinterpret_cast<char*>(malloc(rsize)))){
		ERR_K2HFTPRN("Could not allocate memory.");
		return NULL;
	}

	char*	ptmp	= NULL;
	size_t	setpos	= 0;
	for(k2hftreplist_t::const_iterator iter = replist.begin(); iter != replist.end(); ++iter){
		if(K2HTFT_MAX_REG_MATCH <= (*iter)->matchno){
			// why?
			ERR_K2HFTPRN("Replace position(%d) is over %d, it should be error at loading ini file.", (*iter)->matchno, K2HTFT_MAX_REG_MATCH);
			K2HFT_FREE(presult);
			return NULL;

		}else if(0 > (*iter)->matchno){
			regmatch_t	tmpmatch = {0, static_cast<regoff_t>((*iter)->substring.length())};
			if(NULL == (ptmp = append_substring(presult, setpos, rsize, (*iter)->substring.c_str(), (*iter)->substring.length(), tmpmatch))){
				ERR_K2HFTPRN("Failed to replace static string(%s).", (*iter)->substring.c_str());
				K2HFT_FREE(presult);
				return NULL;
			}
			presult = ptmp;

		}else{	// 0 <= (*iter)->matchno <= 9
			if(NULL == (ptmp = append_substring(presult, setpos, rsize, reinterpret_cast<const char*>(data), length, pmatches[(*iter)->matchno]))){
				ERR_K2HFTPRN("Failed to replace position(%d).", (*iter)->matchno);
				K2HFT_FREE(presult);
				return NULL;
			}
			presult = ptmp;
		}
	}

	// terminate data(add CR/LF and '\0' character to end)
	// setpos is set position which is next pos after '\0' char.
	if(NULL == (ptmp = append_lastword(presult, setpos, rsize, reinterpret_cast<const char*>(data), length))){
		ERR_K2HFTPRN("Failed to append last words and terminate.");
		K2HFT_FREE(presult);
		return NULL;
	}
	presult		= ptmp;
	outputlen	= setpos;

	return reinterpret_cast<unsigned char*>(presult);
}

//
// Return:
//	NULL		Something error occurred
//	same pointer as data
//				found by rule, returned pointer not need to free.
//	another pointer
//				found by rule, returned pointer MUST free.
//
// [NOTE]
// If binary mode, rule does not have MatchingList.
//
static unsigned char* convert_to_output(PK2HFTRULE prule, const unsigned char* data, size_t length, size_t& rsize)
{
	// Already check parameters.
	//
	unsigned char*	presult	= NULL;
	rsize					= 0;

	if(prule->DefaultDenyAll){
		// Default is deny for all
		for(k2hftmatchlist_t::const_iterator iter = prule->MatchingList.begin(); iter != prule->MatchingList.end(); ++iter){
			if((*iter)->IsRegex){
				// Regex
				regmatch_t	matches[K2HTFT_MAX_REG_MATCH];
				if(0 == regexec(&((*iter)->RegexBuffer), reinterpret_cast<const char*>(data), K2HTFT_MAX_REG_MATCH, matches, 0)){
					// matched
					if(NULL == (presult = build_replace_result_data((*iter)->ReplaceList, data, length, matches, rsize))){
						ERR_K2HFTPRN("Failed to replace by regex.");
						return NULL;
					}
					break;
				}
			}else{
				// Normal( If BaseString is empty, do transfer)
				if((*iter)->BaseString.empty() || NULL != strstr(reinterpret_cast<const char*>(data), (*iter)->BaseString.c_str())){
					// found
					presult	= const_cast<unsigned char*>(data);
					rsize	= length;
					break;
				}
			}
		}
	}else{
		// Default is allow for all
		for(k2hftmatchlist_t::const_iterator iter = prule->MatchingList.begin(); iter != prule->MatchingList.end(); ++iter){
			if((*iter)->IsRegex){
				// Regex
				regmatch_t	matches[K2HTFT_MAX_REG_MATCH];
				if(0 == regexec(&((*iter)->RegexBuffer), reinterpret_cast<const char*>(data), K2HTFT_MAX_REG_MATCH, matches, 0)){
					// matched -> deny
					return presult;
				}
			}else{
				// Normal( If BaseString is empty, do transfer)
				if((*iter)->BaseString.empty() || NULL != strstr(reinterpret_cast<const char*>(data), (*iter)->BaseString.c_str())){
					// found -> deny
					return presult;
				}
			}
		}
		// not found -> allow
		presult	= const_cast<unsigned char*>(data);
		rsize	= length;
	}
	return presult;
}

//---------------------------------------------------------
// K2hFtInfo
//---------------------------------------------------------
K2hFtInfo::K2hFtInfo(const char* conffile, mode_t init_mode, mode_t init_dmode, uid_t init_uid, gid_t init_gid, K2hFtPluginMan* ppluginman) : 
		prule_lockval(NULL), Config(""), IsMemType(true), K2hFilePath(""), IsInitialize(true), IsFullmap(true),
		MaskBitCnt(8), CMaskBitCnt(4), MaxElementCnt(32), PageSize(128), DtorThreadCnt(1), DtorCtpPath(K2HFT_K2HTPDTOR),
		IsBinaryMode(false), ExpireTime(0), DirMode(0), DirUid(0), DirGid(0), InitTime(common_init_time)
{
	// cppcheck-suppress unmatchedSuppression
	// cppcheck-suppress noOperatorEq
	// cppcheck-suppress noCopyConstructor
	prule_lockval	= new int;
	*prule_lockval	= FLCK_NOSHARED_MUTEX_VAL_UNLOCKED;

	// set InitTime now.
	if(!set_now_timespec(InitTime)){
		WAN_K2HFTPRN("Failed to initialize start time, but continue...");
	}
	if(!K2HFT_ISEMPTYSTR(conffile) && ppluginman){
		Load(conffile, init_mode, init_dmode, init_uid, init_gid, *ppluginman);
	}
}

K2hFtInfo::~K2hFtInfo(void)
{
	Clean();
	K2HFT_DELETE(prule_lockval);
}

void K2hFtInfo::Clean(void)
{
	Config			= "";
	IsMemType		= true;
	K2hFilePath		= "";
	IsInitialize	= true;
	IsFullmap		= true;
	MaskBitCnt		= 8;
	CMaskBitCnt		= 4;
	MaxElementCnt	= 32;
	PageSize		= 128;
	DtorThreadCnt	= 1;
	DtorCtpPath		= K2HFT_K2HTPDTOR;
	DirMode			= 0;
	DirUid			= 0;
	DirGid			= 0;

	// Lock
	while(!fullock::flck_trylock_noshared_mutex(prule_lockval));

	for(k2hftrulemap_t::iterator iter = RuleMap.begin(); iter != RuleMap.end(); ++iter){
		K2HFT_DELETE(iter->second);
	}
	RuleMap.clear();

	// Unlock
	fullock::flck_unlock_noshared_mutex(prule_lockval);

	// reset InitTime
	if(!set_now_timespec(InitTime)){
		WAN_K2HFTPRN("Failed to initialize start time, but continue...");
	}
}

bool K2hFtInfo::InitializeOutputFiles(void) const
{
	for(k2hftrulemap_t::const_iterator iter = RuleMap.begin(); iter != RuleMap.end(); ++iter){
		PK2HFTRULE	pRule = iter->second;
		if(!pRule->OutputPath.empty()){
			// found output path
			if(S_ISDIR(pRule->Mode) && '/' == pRule->OutputPath[pRule->OutputPath.length() - 1]){
				// output path is directory
				// cppcheck-suppress unmatchedSuppression
				// cppcheck-suppress stlIfStrFind
				if('/' == pRule->OutputPath[0] && 0 != pRule->OutputPath.find(K2hFtManage::GetMountPoint())){
					// directory is not under mount point

					// make directory
					string	dirpath = pRule->OutputPath.substr(0, pRule->OutputPath.length() - 1);	// cut last '/'
					string	abspath;
					if(!mkdir_by_abs_path(dirpath.c_str(), abspath)){
						ERR_K2HFTPRN("Rule(target:%s) specified output directory(%s), but could not make directory.", pRule->TargetPath.c_str(), pRule->OutputPath.c_str());
						return false;
					}
					// output directory is OK
					pRule->OutputPath = abspath + '/';												// last word '/'

				}else{
					// directory under mount point
					string	tmppath;
					if('/' != pRule->OutputPath[0]){
						// convert full path( mountpoint/... )
						tmppath				= K2hFtManage::GetMountPoint();							// mount point is not terminated '/'
						tmppath				+= '/';
						tmppath				+= pRule->OutputPath;
						pRule->OutputPath	= tmppath;												// '/' terminated
					}
					tmppath = pRule->OutputPath.substr(strlen(K2hFtManage::GetMountPoint()) + 1);	// relative path(with '/' terminated)

					// search directory in all rule's target
					bool	found = false;
					for(k2hftrulemap_t::const_iterator iter2 = RuleMap.begin(); iter2 != RuleMap.end(); ++iter2){
						PK2HFTRULE	pRule2 = iter2->second;
						if(!S_ISDIR(pRule2->Mode)){
							// make full path for rule2
							string	tgfullpath;
							if('/' != pRule2->TargetPath[0]){
								// convert full path( mountpoint/... )
								tgfullpath	= K2hFtManage::GetMountPoint();								// mount point is not terminated '/'
								tgfullpath	+= '/';
								tgfullpath	+= pRule2->TargetPath;
							}else{
								tgfullpath	= pRule2->TargetPath;
							}

							// get parent directory
							string	dirpath;
							if(!get_parent_dirpath(tgfullpath.c_str(), dirpath)){
								ERR_K2HFTPRN("Rule target(%s) path is something wrong.", pRule2->TargetPath.c_str());
								return false;
							}
							dirpath += '/';

							if(dirpath == pRule->OutputPath){
								// found same directory
								found = true;
								break;
							}

						}else{
							// need to same directory
							string	dirpath = pRule2->TargetPath;
							if('/' != dirpath[dirpath.length() - 1]){
								dirpath += '/';
							}
							if(dirpath == tmppath){
								// found same directory
								found = true;
								break;
							}
						}
					}
					if(!found){
						ERR_K2HFTPRN("Rule(target:%s) specified output directory(%s), but it under mount point is not specified in rule in INI file.", pRule->TargetPath.c_str(), pRule->OutputPath.c_str());
						return false;
					}
					// output directory is OK
				}

			}else{
				// output path is file
				// cppcheck-suppress unmatchedSuppression
				// cppcheck-suppress stlIfStrFind
				if('/' == pRule->OutputPath[0] && 0 != pRule->OutputPath.find(K2hFtManage::GetMountPoint())){
					// file is not under mount point
					if('/' == pRule->OutputPath[pRule->OutputPath.length() - 1]){
						ERR_K2HFTPRN("Rule(target:%s) specified output file(%s), but file path is directory.", pRule->TargetPath.c_str(), pRule->OutputPath.c_str());
						return false;
					}
					string	dirpath;
					if(!get_parent_dirpath(pRule->OutputPath.c_str(), dirpath)){
						ERR_K2HFTPRN("Rule(target:%s) specified output file(%s), but file path is something wrong.", pRule->TargetPath.c_str(), pRule->OutputPath.c_str());
						return false;
					}
					// make directory
					string	abspath;
					if(!mkdir_by_abs_path(dirpath.c_str(), abspath)){
						ERR_K2HFTPRN("Rule(target:%s) specified output file(%s), but could not make directory %s.", pRule->TargetPath.c_str(), pRule->OutputPath.c_str(), dirpath.c_str());
						return false;
					}
					// check file
					if(!make_file_by_abs_path(pRule->OutputPath.c_str(), pRule->Mode, abspath)){
						ERR_K2HFTPRN("Rule(target:%s) specified output file(%s), but could not make(initialize) output file.", pRule->TargetPath.c_str(), pRule->OutputPath.c_str());
						return false;
					}
					// output file is OK
					pRule->OutputPath = abspath;

				}else{
					// file under mount point
					string	tmppath;
					if('/' != pRule->OutputPath[0]){
						// convert full path( mountpoint/... )
						tmppath				= K2hFtManage::GetMountPoint();							// mount point is not terminated '/'
						tmppath				+= '/';
						tmppath				+= pRule->OutputPath;
						pRule->OutputPath	= tmppath;
					}
					tmppath = pRule->OutputPath.substr(strlen(K2hFtManage::GetMountPoint()) + 1);	// relative path

					// search path in all rule's target
					bool	found = false;
					for(k2hftrulemap_t::const_iterator iter2 = RuleMap.begin(); iter2 != RuleMap.end(); ++iter2){
						PK2HFTRULE	pRule2 = iter2->second;
						if(!S_ISDIR(pRule2->Mode)){
							// need to same
							if(pRule2->TargetPath == tmppath){
								found = true;
								break;
							}
						}else{
							// need to same directory
							// cppcheck-suppress stlIfStrFind
							if(pRule2->TargetPath != tmppath && 0 == tmppath.find(pRule2->TargetPath) && '/' == tmppath[pRule2->TargetPath.length()]){
								// same directory
								string	tmpfile = tmppath.substr(pRule2->TargetPath.length() + 1);
								if(!tmpfile.empty() && string::npos == tmpfile.find('/')){
									// file name does not have '/', so OK.
									found = true;
									break;
								}
							}
						}
					}
					if(!found){
						ERR_K2HFTPRN("Rule(target:%s) specified output file(%s), but file under mount point is not specified in rule in INI file.", pRule->TargetPath.c_str(), pRule->OutputPath.c_str());
						return false;
					}
					// output file is OK
				}
			}
		}
	}
	return true;
}

bool K2hFtInfo::CheckBinaryMode(void)
{
	if(!IsBinaryMode){
		// nothing to check
		return true;
	}
	for(k2hftrulemap_t::const_iterator iter = RuleMap.begin(); iter != RuleMap.end(); ++iter){
		PK2HFTRULE	pRule = iter->second;
		if(0 != pRule->MatchingList.size()){
			ERR_K2HFTPRN("Rule for %s has matching list, but the process run BINARY TRANSFER MODE, Thus ALL MATCHING RULE is ignored!!!", pRule->TargetPath.c_str());
			free_match_list(pRule->MatchingList);
		}
	}
	return true;
}

//
// Do not need to lock in this method, because caller locks before calling.
//
bool K2hFtInfo::Load(const char* config, mode_t init_mode, mode_t init_dmode, uid_t init_uid, gid_t init_gid, K2hFtPluginMan& pluginman)
{
	// get configuration type without environment
	CHMCONFTYPE	conftype = check_chmconf_type(config);
	if(CHMCONF_TYPE_UNKNOWN == conftype || CHMCONF_TYPE_NULL == conftype){
		ERR_K2HFTPRN("Parameter configuration file or json string is wrong.");
		return false;
	}

	bool	result;
	if(CHMCONF_TYPE_INI_FILE == conftype){
		result = LoadIni(config, init_mode, init_dmode, init_uid, init_gid, pluginman);
	}else{
		result = LoadYaml(config, init_mode, init_dmode, init_uid, init_gid, pluginman, (CHMCONF_TYPE_JSON_STRING == conftype));
	}
	return result;
}

bool K2hFtInfo::LoadIni(const char* conffile, mode_t init_mode, mode_t init_dmode, uid_t init_uid, gid_t init_gid, K2hFtPluginMan& pluginman)
{
	if(K2HFT_ISEMPTYSTR(conffile)){
		ERR_K2HFTPRN("conffile is empty.");
		return false;
	}
	if(!Config.empty()){
		ERR_K2HFTPRN("Already load configuration file, so could not load %s file.", conffile);
		return false;
	}

	strlst_t	lines;
	strlst_t	files;
	if(!read_ini_file_contents(conffile, lines, files)){
		ERR_K2HFTPRN("Failed to load configuration ini file(%s)", conffile);
		return false;
	}

	// Parse & Check
	bool	set_main_sec = false;
	for(strlst_t::const_iterator iter = lines.begin(); iter != lines.end(); ){
		if((*iter) == INI_K2HFT_MAIN_SEC_STR){
			// main section
			if(set_main_sec){
				ERR_K2HFTPRN("main section(%s) is already set.", INI_K2HFT_MAIN_SEC_STR);
				Clean();
				return false;
			}
			set_main_sec = true;

			// loop for main section
			for(++iter; iter != lines.end(); ++iter){
				if(INI_SEC_START_CHAR == (*iter)[0] && INI_SEC_END_CHAR == (*iter)[iter->length() - 1]){
					// another section start, so break this loop
					break;
				}

				// parse key(to upper) & value
				string	key;
				string	value;
				parse_ini_line((*iter), key, value);

				if(INI_K2HFT_K2HTYPE_STR == key){
					// IsMemType
					value = upper(value);
					if(INI_K2HFT_MEM1_VAL_STR == value || INI_K2HFT_MEM2_VAL_STR == value || INI_K2HFT_MEM3_VAL_STR == value){
						IsMemType	= true;
						K2hFilePath	= "";	// clear file path
					}else if(INI_K2HFT_TEMP1_VAL_STR == value || INI_K2HFT_TEMP2_VAL_STR == value || INI_K2HFT_TEMP3_VAL_STR == value){
						IsMemType	= false;
						K2hFilePath	= "";	// clear file path
					}else if(INI_K2HFT_FILE1_VAL_STR == value || INI_K2HFT_FILE2_VAL_STR == value){
						IsMemType	= false;
					}else{
						WAN_K2HFTPRN("keyword(%s)'s value(%s) in main section(%s) is unknown value, so skip it.", key.c_str(), value.c_str(), INI_K2HFT_MAIN_SEC_STR);
					}

				}else if(INI_K2HFT_K2HFILE_STR == key){
					// K2hFilePath
					K2hFilePath = "";
					if(!check_path_real_path(value.c_str(), K2hFilePath)){
						MSG_K2HFTPRN("keyword(%s)'s value(%s) in main section(%s) does not exist.", key.c_str(), value.c_str(), INI_K2HFT_MAIN_SEC_STR);
						// not abspath.
						K2hFilePath = value;
					}

				}else if(INI_K2HFT_K2HFULLMAP_STR == key){
					// IsFullmap
					value = upper(value);
					if(INI_K2HFT_YES1_VAL_STR == value || INI_K2HFT_YES2_VAL_STR == value || INI_K2HFT_ON_VAL_STR == value){
						IsFullmap = true;
					}else if(INI_K2HFT_NO1_VAL_STR == value || INI_K2HFT_NO2_VAL_STR == value || INI_K2HFT_OFF_VAL_STR == value){
						IsFullmap = false;
					}else{
						WAN_K2HFTPRN("keyword(%s)'s value(%s) in main section(%s) is unknown value, so skip it.", key.c_str(), value.c_str(), INI_K2HFT_MAIN_SEC_STR);
					}

				}else if(INI_K2HFT_K2HINIT_STR == key){
					// IsInitialize
					value = upper(value);
					if(INI_K2HFT_YES1_VAL_STR == value || INI_K2HFT_YES2_VAL_STR == value || INI_K2HFT_ON_VAL_STR == value){
						IsInitialize = true;
					}else if(INI_K2HFT_NO1_VAL_STR == value || INI_K2HFT_NO2_VAL_STR == value || INI_K2HFT_OFF_VAL_STR == value){
						IsInitialize = false;
					}else{
						WAN_K2HFTPRN("keyword(%s)'s value(%s) in main section(%s) is unknown value, so skip it.", key.c_str(), value.c_str(), INI_K2HFT_MAIN_SEC_STR);
					}

				}else if(INI_K2HFT_K2HMASKBIT_STR == key){
					// MaskBitCnt
					MaskBitCnt = atoi(value.c_str());

				}else if(INI_K2HFT_K2HCMASKBIT_STR == key){
					// CMaskBitCnt
					CMaskBitCnt = atoi(value.c_str());

				}else if(INI_K2HFT_K2HMAXELE_STR == key){
					// MaxElementCnt
					MaxElementCnt = atoi(value.c_str());

				}else if(INI_K2HFT_K2HPAGESIZE_STR == key){
					// PageSize
					PageSize = static_cast<size_t>(atoll(value.c_str()));

				}else if(INI_K2HFT_DTORTHREADCNT_STR == key){
					// DtorThreadCnt
					int	nTmp = atoi(value.c_str());
					if(0 >= nTmp){
						ERR_K2HFTPRN("keyword(%s)'s value(%s) in main section(%s) must be over 0.", key.c_str(), value.c_str(), INI_K2HFT_MAIN_SEC_STR);
						Clean();
						return false;
					}
					DtorThreadCnt = nTmp;

				}else if(INI_K2HFT_DTORCTP_STR == key){
					// DtorCtpPath
					DtorCtpPath = K2HFT_K2HTPDTOR;
					if(!check_path_real_path(value.c_str(), DtorCtpPath)){
						ERR_K2HFTPRN("keyword(%s)'s value(%s) in main section(%s) does not exist.", key.c_str(), value.c_str(), INI_K2HFT_MAIN_SEC_STR);
						Clean();
						return false;
					}

				}else if(INI_K2HFT_BINTRANS_STR == key){
					// Binary Transfer Mode
					value = upper(value);
					if(INI_K2HFT_YES1_VAL_STR == value || INI_K2HFT_YES2_VAL_STR == value || INI_K2HFT_ON_VAL_STR == value){
						IsBinaryMode = true;
					}else if(INI_K2HFT_NO1_VAL_STR == value || INI_K2HFT_NO2_VAL_STR == value || INI_K2HFT_OFF_VAL_STR == value){
						IsBinaryMode = false;
					}else{
						WAN_K2HFTPRN("keyword(%s)'s value(%s) in main section(%s) is unknown value, so skip it.", key.c_str(), value.c_str(), INI_K2HFT_MAIN_SEC_STR);
					}

				}else if(INI_K2HFT_EXPIRE_STR == key){
					// Expire seconds
					time_t	expire_sec = static_cast<time_t>(atoi(value.c_str()));
					if(expire_sec <= 0){
						ERR_K2HFTPRN("keyword(%s)'s value(%s) in main section(%s) must be over 0.", key.c_str(), value.c_str(), INI_K2HFT_MAIN_SEC_STR);
						Clean();
						return false;
					}
					ExpireTime = expire_sec;

				}else if(INI_K2HFT_TRANSLINECNT_STR == key){
					// Minimum transfer line limit
					int	linecnt = atoi(value.c_str());
					if(linecnt < 0){
						ERR_K2HFTPRN("keyword(%s)'s value(%s) in main section(%s) must be a positive number including 0.", key.c_str(), value.c_str(), INI_K2HFT_MAIN_SEC_STR);
						Clean();
						return false;
					}
					K2hFtWriteBuff::SetStackLineMax(static_cast<size_t>(linecnt));

				}else if(INI_K2HFT_TRANSTIMEUP_STR == key){
					// transfer timeup limit
					int	timeup = atoi(value.c_str());
					if(timeup < 0){
						ERR_K2HFTPRN("keyword(%s)'s value(%s) in main section(%s) must be a positive number including 0.", key.c_str(), value.c_str(), INI_K2HFT_MAIN_SEC_STR);
						Clean();
						return false;
					}
					K2hFtWriteBuff::SetStackTimeup(static_cast<time_t>(timeup));

				}else if(INI_K2HFT_BYTELIMIT_STR == key){
					// transfer timeup limit
					int	limit = atoi(value.c_str());
					if(limit < 0){
						ERR_K2HFTPRN("keyword(%s)'s value(%s) in main section(%s) must be a positive number including 0.", key.c_str(), value.c_str(), INI_K2HFT_MAIN_SEC_STR);
						Clean();
						return false;
					}
					K2hFtWriteBuff::SetLineByteLimit(static_cast<size_t>(limit));

				}else{
					WAN_K2HFTPRN("Unknown keyword(%s) in main section(%s), so skip it and continue...", key.c_str(), INI_K2HFT_MAIN_SEC_STR);
				}
			}

			// last check for k2hash type
			if(IsMemType){
				if(!K2hFilePath.empty()){
					ERR_K2HFTPRN("Specified memory type k2hash, but k2hash file path(%s) is specified.", K2hFilePath.c_str());
					Clean();
					return false;
				}
			}

		}else if((*iter) == INI_K2HFT_RULEDIR_SEC_STR || (*iter) == INI_K2HFT_RULE_SEC_STR){
			bool	is_dir_rule = ((*iter) == INI_K2HFT_RULEDIR_SEC_STR);

			// RuleMap(Rule or RuleDir)
			PK2HFTRULE			pTmpRule	= new K2HFTRULE;
			k2hftmatchlist_t	AllowMatchs;
			k2hftmatchlist_t	DenyMatchs;

			// loop for rule and ruledir section
			for(++iter; iter != lines.end(); ++iter){
				if(INI_SEC_START_CHAR == (*iter)[0] && INI_SEC_END_CHAR == (*iter)[iter->length() - 1]){
					// another section start, so break this loop
					break;
				}

				// parse key(to upper) & value
				string	key;
				string	value;
				parse_ini_line((*iter), key, value);

				if(INI_K2HFT_TARGET_STR == key){
					// K2HFTRULE::TargetPath
					pTmpRule->TargetPath = value;

				}else if(INI_K2HFT_TRUNS_STR == key){
					// K2HFTRULE::IsTransfer
					value = upper(value);
					if(INI_K2HFT_YES1_VAL_STR == value || INI_K2HFT_YES2_VAL_STR == value || INI_K2HFT_ON_VAL_STR == value){
						pTmpRule->IsTransfer = true;
					}else if(INI_K2HFT_NO1_VAL_STR == value || INI_K2HFT_NO2_VAL_STR == value || INI_K2HFT_OFF_VAL_STR == value){
						pTmpRule->IsTransfer = false;
					}else{
						WAN_K2HFTPRN("keyword(%s)'s value(%s) in rule section(%s) is unknown value, so skip it.", key.c_str(), value.c_str(), (is_dir_rule ? INI_K2HFT_RULEDIR_SEC_STR : INI_K2HFT_RULE_SEC_STR));
					}

				}else if(INI_K2HFT_OUTPUTFILE_STR == key){
					// K2HFTRULE::OutputPath
					if("/dev/null" == value){
						pTmpRule->OutputPath = "";			// not put to file
					}else{
						pTmpRule->OutputPath = value;
					}

				}else if(INI_K2HFT_PLUGIN_STR == key){
					// K2HFTRULE::pPlugin
					if(pTmpRule->pPlugin){
						WAN_K2HFTPRN("keyword(%s) in rule section(%s) is already set, then it is over wrote by new value(%s),", key.c_str(), (is_dir_rule ? INI_K2HFT_RULEDIR_SEC_STR : INI_K2HFT_RULE_SEC_STR), value.c_str());
						K2HFT_DELETE(pTmpRule->pPlugin);
					}
					pTmpRule->pPlugin			= new K2HFT_PLUGIN;
					pTmpRule->pPlugin->BaseParam= value;

					// add plugin to manager
					if(!pluginman.AddPluginInfo(pTmpRule->pPlugin)){
						ERR_K2HFTPRN("keyword(%s) in rule section(%s) has plugin, but failed to set it to plugin manager.", key.c_str(), (is_dir_rule ? INI_K2HFT_RULEDIR_SEC_STR : INI_K2HFT_RULE_SEC_STR));
						K2HFT_DELETE(pTmpRule->pPlugin);
					}

				}else if(INI_K2HFT_DEFAULTALL_STR == key){
					// K2HFTRULE::DefaultDenyAll
					value = upper(value);
					if(INI_K2HFT_DENY_VAL_STR == value){
						pTmpRule->DefaultDenyAll = true;
					}else if(INI_K2HFT_ALLOW_VAL_STR == value){
						pTmpRule->DefaultDenyAll = false;
					}else{
						WAN_K2HFTPRN("keyword(%s)'s value(%s) in rule section(%s) is unknown value, so skip it.", key.c_str(), value.c_str(), (is_dir_rule ? INI_K2HFT_RULEDIR_SEC_STR : INI_K2HFT_RULE_SEC_STR));
					}

				}else if(INI_K2HFT_ALLOW_STR == key || INI_K2HFT_DENY_STR == key){
					// ALLOW / DENY
					bool	is_key_allow = false;
					if(INI_K2HFT_ALLOW_STR == key){
						is_key_allow = true;
					}
					strlst_t	values;
					parse_ini_value(value, values);

					// cppcheck-suppress unmatchedSuppression
					// cppcheck-suppress stlSize
					if(0 == values.size()){
						// any pattern
						MSG_K2HFTPRN("keyword(%s)'s value(%s) in rule section(%s) has nothing, this is any pattern hits.", key.c_str(), value.c_str(), (is_dir_rule ? INI_K2HFT_RULEDIR_SEC_STR : INI_K2HFT_RULE_SEC_STR));

						PK2HFTMATCHPTN	pMatchPtn	= new K2HFTMATCHPTN;
						pMatchPtn->IsRegex 			= false;
						if(is_key_allow){
							AllowMatchs.push_back(pMatchPtn);
						}else{
							DenyMatchs.push_back(pMatchPtn);
						}

					}else if(2 < values.size()){
						WAN_K2HFTPRN("keyword(%s)'s value(%s) in rule section(%s) has nothing or too many elements, so skip it.", key.c_str(), value.c_str(), (is_dir_rule ? INI_K2HFT_RULEDIR_SEC_STR : INI_K2HFT_RULE_SEC_STR));

					}else{
						string			strcompptn	= values.front();
						values.pop_front();

						// cppcheck-suppress unmatchedSuppression
						// cppcheck-suppress stlSize
						string			strrepptn	= 0 < values.size() ? values.front() : "";
						PK2HFTMATCHPTN	pMatchPtn	= build_k2hft_match_pattern(strcompptn.c_str(), strrepptn.c_str());
						if(!pMatchPtn){
							WAN_K2HFTPRN("keyword(%s)'s value(%s) in rule section(%s) is something wrong, so skip it.", key.c_str(), value.c_str(), (is_dir_rule ? INI_K2HFT_RULEDIR_SEC_STR : INI_K2HFT_RULE_SEC_STR));
						}else{
							if(is_key_allow){
								AllowMatchs.push_back(pMatchPtn);
							}else{
								DenyMatchs.push_back(pMatchPtn);
							}
						}
					}

				}else{
					WAN_K2HFTPRN("Unknown keyword(%s) in rule section(%s), so skip it and continue...", key.c_str(), (is_dir_rule ? INI_K2HFT_RULEDIR_SEC_STR : INI_K2HFT_RULE_SEC_STR));
				}
			}

			// set Matching List
			if(pTmpRule->DefaultDenyAll){
				// check deny list
				if(0 < DenyMatchs.size()){
					WAN_K2HFTPRN("rule section(%s) is Default Deny from All, but it has some DENY keywords, so skip those and continue...", (is_dir_rule ? INI_K2HFT_RULEDIR_SEC_STR : INI_K2HFT_RULE_SEC_STR));

					for(k2hftmatchlist_t::iterator iter2 = DenyMatchs.begin(); iter2 != DenyMatchs.end(); ++iter2){
						K2HFT_DELETE(*iter2);
					}
					DenyMatchs.clear();
				}
				// copy allow list
				for(k2hftmatchlist_t::iterator iter3 = AllowMatchs.begin(); iter3 != AllowMatchs.end(); ++iter3){
					pTmpRule->MatchingList.push_back(*iter3);
				}
				AllowMatchs.clear();

			}else{
				// check allow list
				if(0 < AllowMatchs.size()){
					WAN_K2HFTPRN("rule section(%s) is Default Deny from All, but it has some DENY keywords, so skip those and continue...", (is_dir_rule ? INI_K2HFT_RULEDIR_SEC_STR : INI_K2HFT_RULE_SEC_STR));

					for(k2hftmatchlist_t::iterator iter2 = AllowMatchs.begin(); iter2 != AllowMatchs.end(); ++iter2){
						K2HFT_DELETE(*iter2);
					}
					AllowMatchs.clear();
				}
				// copy deny list
				for(k2hftmatchlist_t::iterator iter3 = DenyMatchs.begin(); iter3 != DenyMatchs.end(); ++iter3){
					pTmpRule->MatchingList.push_back(*iter3);
				}
				DenyMatchs.clear();
			}

			// check plugin and set output file/execute flag
			if(pTmpRule->pPlugin){
				pTmpRule->pPlugin->mode			= is_dir_rule ? init_dmode : init_mode;
				pTmpRule->pPlugin->not_execute	= is_dir_rule;

				// [NOTE]
				// If output path is relative path, convert it to absolute path for plugin here.
				// Original output path will be convert absolute path in InitializeOutputFiles().
				//
				if('/' != pTmpRule->OutputPath[0]){
					// output path is relative, so convert to absolute( mountpoint/... )
					pTmpRule->pPlugin->OutputPath	= K2hFtManage::GetMountPoint();							// mount point is not terminated '/'
					pTmpRule->pPlugin->OutputPath	+= '/';
					pTmpRule->pPlugin->OutputPath	+= pTmpRule->OutputPath;
				}else{
					pTmpRule->pPlugin->OutputPath	= pTmpRule->OutputPath;
				}
			}

			// set other member
			pTmpRule->Mode		= is_dir_rule ? init_dmode : init_mode;
			pTmpRule->Uid		= init_uid;
			pTmpRule->Gid		= init_gid;
			pTmpRule->AccumSize	= is_dir_rule ? static_cast<off_t>(get_system_pagesize()) : 0;
			pTmpRule->StartTime	= InitTime;
			pTmpRule->LastTime	= InitTime;

			// set into RuleMap
			RuleMap[pTmpRule->TargetPath] = pTmpRule;

		}else if(INI_SEC_START_CHAR == (*iter)[0] && INI_SEC_END_CHAR == (*iter)[iter->length() - 1]){
			// Unknown section.
			for(++iter; iter != lines.end(); ++iter){
				if(INI_SEC_START_CHAR == (*iter)[0] && INI_SEC_END_CHAR == (*iter)[iter->length() - 1]){
					// another section start, so break this loop
					break;
				}
			}

		}else{
			// why?(no section)
			++iter;
		}
	}

	// check main values
	if(IsMemType && !K2hFilePath.empty()){
		ERR_K2HFTPRN("k2hash type is memory, but specified k2hash file path(%s).", K2hFilePath.c_str());
		Clean();
		return false;
	}
	if(K2hFilePath.empty() && !IsInitialize){
		ERR_K2HFTPRN("k2hash type is %s, but specified initialize flag as false.", IsMemType ? "memory" : "temporary file");
		Clean();
		return false;
	}
	if(K2hFilePath.empty() && !IsFullmap){
		ERR_K2HFTPRN("k2hash type is %s, but specified full mapping flag as false.", IsMemType ? "memory" : "temporary file");
		Clean();
		return false;
	}
	// check and prepare for output files
	if(!InitializeOutputFiles()){
		ERR_K2HFTPRN("Could not initialize(make) some rule output file or directory.");
		Clean();
		return false;
	}
	// check binary mode
	CheckBinaryMode();

	// set
	Config	= conffile;
	DirMode	= init_dmode;
	DirUid	= init_uid;
	DirGid	= init_gid;

	return true;
}

bool K2hFtInfo::LoadYaml(const char* config, mode_t init_mode, mode_t init_dmode, uid_t init_uid, gid_t init_gid, K2hFtPluginMan& pluginman, bool is_json_string)
{
	// initialize yaml parser
	yaml_parser_t	yparser;
	if(!yaml_parser_initialize(&yparser)){
		ERR_K2HFTPRN("Failed to initialize yaml parser");
		return false;
	}

	FILE*	fp = NULL;
	if(!is_json_string){
		// open configuration file
		if(NULL == (fp = fopen(config, "r"))){
			ERR_K2HFTPRN("Could not open configuration file(%s). errno = %d", config, errno);
			return false;
		}
		// set file to parser
		yaml_parser_set_input_file(&yparser, fp);

	}else{	// JSON_STR
		// set string to parser
		yaml_parser_set_input_string(&yparser, reinterpret_cast<const unsigned char*>(config), strlen(config));
	}

	// Do parsing
	bool	result	= LoadYamlTopLevel(yparser, init_mode, init_dmode, init_uid, init_gid, pluginman);

	yaml_parser_delete(&yparser);
	if(fp){
		fclose(fp);
	}
	if(result){
		Config	= config;
		DirMode	= init_dmode;
		DirUid	= init_uid;
		DirGid	= init_gid;
	}
	return result;
}

bool K2hFtInfo::LoadYamlTopLevel(yaml_parser_t& yparser, mode_t init_mode, mode_t init_dmode, uid_t init_uid, gid_t init_gid, K2hFtPluginMan& pluginman)
{
	CHMYamlDataStack	other_stack;
	bool				is_set_main	= false;
	bool				result		= true;
	for(bool is_loop = true, in_stream = false, in_document = false, in_toplevel = false; is_loop && result; ){
		// get event
		yaml_event_t	yevent;
		if(!yaml_parser_parse(&yparser, &yevent)){
			ERR_K2HFTPRN("Could not parse event. errno = %d", errno);
			result = false;
			continue;
		}

		// check event
		switch(yevent.type){
			case YAML_NO_EVENT:
				MSG_K2HFTPRN("There is no yaml event in loop");
				break;

			case YAML_STREAM_START_EVENT:
				if(!other_stack.empty()){
					MSG_K2HFTPRN("Found start yaml stream event in skipping event loop");
					if(!other_stack.add(yevent.type)){
						result = false;
					}
				}else if(in_stream){
					MSG_K2HFTPRN("Already start yaml stream event in loop, Thus stacks this event.");
					if(!other_stack.add(yevent.type)){
						result = false;
					}
				}else{
					MSG_K2HFTPRN("Start yaml stream event in loop");
					in_stream = true;
				}
				break;

			case YAML_STREAM_END_EVENT:
				if(!other_stack.empty()){
					MSG_K2HFTPRN("Found stop yaml stream event in skipping event loop");
					if(!other_stack.add(yevent.type)){
						result = false;
					}
				}else if(!in_stream){
					MSG_K2HFTPRN("Already stop yaml stream event in loop, Thus stacks this event.");
					if(!other_stack.add(yevent.type)){
						result = false;
					}
				}else{
					MSG_K2HFTPRN("Stop yaml stream event in loop");
					in_stream = false;
				}
				break;

			case YAML_DOCUMENT_START_EVENT:
				if(!other_stack.empty()){
					MSG_K2HFTPRN("Found start yaml document event in skipping event loop");
					if(!other_stack.add(yevent.type)){
						result = false;
					}
				}else if(!in_stream){
					MSG_K2HFTPRN("Found start yaml document event before yaml stream event in loop, Thus stacks this event.");
					if(!other_stack.add(yevent.type)){
						result = false;
					}
				}else if(in_document){
					MSG_K2HFTPRN("Already start yaml document event in loop, Thus stacks this event.");
					if(!other_stack.add(yevent.type)){
						result = false;
					}
				}else{
					MSG_K2HFTPRN("Start yaml document event in loop");
					in_document = true;
				}
				break;

			case YAML_DOCUMENT_END_EVENT:
				if(!other_stack.empty()){
					MSG_K2HFTPRN("Found stop yaml document event in skipping event loop");
					if(!other_stack.add(yevent.type)){
						result = false;
					}
				}else if(!in_document){
					MSG_K2HFTPRN("Already stop yaml document event in loop, Thus stacks this event.");
					if(!other_stack.add(yevent.type)){
						result = false;
					}
				}else{
					MSG_K2HFTPRN("Stop yaml document event in loop");
					in_document = false;
				}
				break;

			case YAML_MAPPING_START_EVENT:
				if(!other_stack.empty()){
					MSG_K2HFTPRN("Found start yaml mapping event in skipping event loop");
					if(!other_stack.add(yevent.type)){
						result = false;
					}
				}else if(!in_stream){
					MSG_K2HFTPRN("Found start yaml mapping event before yaml stream event in loop, Thus stacks this event.");
					if(!other_stack.add(yevent.type)){
						result = false;
					}
				}else if(!in_document){
					MSG_K2HFTPRN("Found start yaml mapping event before yaml document event in loop, Thus stacks this event.");
					if(!other_stack.add(yevent.type)){
						result = false;
					}
				}else if(in_toplevel){
					MSG_K2HFTPRN("Already start yaml mapping event in loop, Thus stacks this event.");
					if(!other_stack.add(yevent.type)){
						result = false;
					}
				}else{
					MSG_K2HFTPRN("Start yaml mapping event in loop");
					in_toplevel = true;
				}
				break;

			case YAML_MAPPING_END_EVENT:
				if(!other_stack.empty()){
					MSG_K2HFTPRN("Found stop yaml mapping event in skipping event loop");
					if(!other_stack.add(yevent.type)){
						result = false;
					}
				}else if(!in_toplevel){
					MSG_K2HFTPRN("Already stop yaml mapping event in loop, Thus stacks this event.");
					if(!other_stack.add(yevent.type)){
						result = false;
					}
				}else{
					MSG_K2HFTPRN("Stop yaml mapping event in loop");
					in_toplevel = false;
				}
				break;

			case YAML_SEQUENCE_START_EVENT:
				// always stacking
				//
				if(!other_stack.empty()){
					MSG_K2HFTPRN("Found start yaml sequence event in skipping event loop");
					if(!other_stack.add(yevent.type)){
						result = false;
					}
				}else{
					MSG_K2HFTPRN("Found start yaml sequence event before top level event in loop, Thus stacks this event.");
					if(!other_stack.add(yevent.type)){
						result = false;
					}
				}
				break;

			case YAML_SEQUENCE_END_EVENT:
				// always stacking
				//
				if(!other_stack.empty()){
					MSG_K2HFTPRN("Found stop yaml sequence event in skipping event loop");
					if(!other_stack.add(yevent.type)){
						result = false;
					}
				}else{
					MSG_K2HFTPRN("Found stop yaml sequence event before top level event in loop, Thus stacks this event.");
					if(!other_stack.add(yevent.type)){
						result = false;
					}
				}
				break;

			case YAML_SCALAR_EVENT:
				if(!other_stack.empty()){
					MSG_K2HFTPRN("Got yaml scalar event in skipping event loop");
					if(!other_stack.add(yevent.type)){
						result = false;
					}
				}else if(!in_stream){
					MSG_K2HFTPRN("Got yaml scalar event before yaml stream event in loop, Thus stacks this event.");
					if(!other_stack.add(yevent.type)){
						result = false;
					}
				}else if(!in_document){
					MSG_K2HFTPRN("Got yaml scalar event before yaml document event in loop, Thus stacks this event.");
					if(!other_stack.add(yevent.type)){
						result = false;
					}
				}else if(!in_toplevel){
					MSG_K2HFTPRN("Got yaml scalar event before yaml mapping event in loop, Thus stacks this event.");
					if(!other_stack.add(yevent.type)){
						result = false;
					}
				}else{
					// Found Top Level Keywords, start to loading
					if(0 == strcasecmp(K2HFT_MAIN_SEC_STR, reinterpret_cast<const char*>(yevent.data.scalar.value))){
						if(is_set_main){
							MSG_K2HFTPRN("Got yaml scalar event in loop, but already loading %s. Thus stacks this event.", K2HFT_MAIN_SEC_STR);
							if(!other_stack.add(yevent.type)){
								result = false;
							}
						}else{
							// Load K2HFT_MAIN_SEC_STR section
							if(!LoadYamlMainSec(yparser, init_mode, init_dmode, init_uid, init_gid, pluginman)){
								ERR_K2HFTPRN("Something error occured in loading %s section.", K2HFT_MAIN_SEC_STR);
								result = false;
							}
						}

					}else{
						MSG_K2HFTPRN("Got yaml scalar event in loop, but unknown keyword(%s) for me. Thus stacks this event.", reinterpret_cast<const char*>(yevent.data.scalar.value));
						if(!other_stack.add(yevent.type)){
							result = false;
						}
					}
				}
				break;

			case YAML_ALIAS_EVENT:
				// [TODO]
				// Now we do not supports alias(anchor) event.
				//
				MSG_K2HFTPRN("Got yaml alias(anchor) event in loop, but we does not support this event. Thus skip this event.");
				break;
		}
		// delete event
		is_loop = yevent.type != YAML_STREAM_END_EVENT;
		yaml_event_delete(&yevent);
	}
	return result;
}

bool K2hFtInfo::LoadYamlMainSec(yaml_parser_t& yparser, mode_t init_mode, mode_t init_dmode, uid_t init_uid, gid_t init_gid, K2hFtPluginMan& pluginman)
{
	// Must start yaml mapping event.
	yaml_event_t	yevent;
	if(!yaml_parser_parse(&yparser, &yevent)){
		ERR_K2HFTPRN("Could not parse event. errno = %d", errno);
		return false;
	}
	if(YAML_MAPPING_START_EVENT != yevent.type){
		ERR_K2HFTPRN("Parsed event type is not start mapping(%d)", yevent.type);
		yaml_event_delete(&yevent);
		return false;
	}
	yaml_event_delete(&yevent);

	// Loading
	string	key("");
	bool	result				= true;
	bool	need_delete_event	= false;
	for(bool is_loop = true; is_loop && result; ){
		// get event
		if(!yaml_parser_parse(&yparser, &yevent)){
			ERR_K2HFTPRN("Could not parse event. errno = %d", errno);
			result = false;
			continue;
		}
		need_delete_event = true;

		// check event
		if(YAML_MAPPING_END_EVENT == yevent.type){
			// End of mapping event
			is_loop = false;

		}else if(YAML_SCALAR_EVENT == yevent.type){
			// Load key & value
			if(key.empty()){
				key = reinterpret_cast<const char*>(yevent.data.scalar.value);

				if(0 == strcasecmp(K2HFT_RULEDIR_SEC_STR, key.c_str()) || 0 == strcasecmp(K2HFT_RULE_SEC_STR, key.c_str())){
					// Load K2HFT_RULEDIR_SEC_STR or K2HFT_RULE_SEC_STR section
					yaml_event_delete(&yevent);
					need_delete_event = false;
					key.clear();

					bool is_dir_rule = (0 == strcasecmp(K2HFT_RULEDIR_SEC_STR, key.c_str()));
					if(!LoadYamlRuleSec(yparser, init_mode, init_dmode, init_uid, init_gid, pluginman, is_dir_rule)){
						ERR_K2HFTPRN("Something error occured in loading %s section.", (is_dir_rule ? K2HFT_RULEDIR_SEC_STR : K2HFT_RULE_SEC_STR));
						result = false;
					}
				}

			}else{
				//
				// Compare key and set value
				//
				if(0 == strcasecmp(INI_K2HFT_K2HTYPE_STR, key.c_str())){
					// K2hash Type
					string	value = upper(reinterpret_cast<const char*>(yevent.data.scalar.value));
					if(INI_K2HFT_MEM1_VAL_STR == value || INI_K2HFT_MEM2_VAL_STR == value || INI_K2HFT_MEM3_VAL_STR == value){
						IsMemType	= true;
						K2hFilePath	= "";	// clear file path
					}else if(INI_K2HFT_TEMP1_VAL_STR == value || INI_K2HFT_TEMP2_VAL_STR == value || INI_K2HFT_TEMP3_VAL_STR == value){
						IsMemType	= false;
						K2hFilePath	= "";	// clear file path
					}else if(INI_K2HFT_FILE1_VAL_STR == value || INI_K2HFT_FILE2_VAL_STR == value){
						IsMemType	= false;
					}else{
						WAN_K2HFTPRN("keyword(%s)'s value(%s) in main section(%s) is unknown value, so skip it.", key.c_str(), reinterpret_cast<const char*>(yevent.data.scalar.value), K2HFT_MAIN_SEC_STR);
					}

				}else if(0 == strcasecmp(INI_K2HFT_K2HFILE_STR, key.c_str())){
					// K2hash file path
					K2hFilePath = "";
					if(!check_path_real_path(reinterpret_cast<const char*>(yevent.data.scalar.value), K2hFilePath)){
						MSG_K2HFTPRN("keyword(%s)'s value(%s) in main section(%s) does not exist.", key.c_str(), reinterpret_cast<const char*>(yevent.data.scalar.value), K2HFT_MAIN_SEC_STR);
						// not abspath.
						K2hFilePath = reinterpret_cast<const char*>(yevent.data.scalar.value);
					}

				}else if(0 == strcasecmp(INI_K2HFT_K2HFULLMAP_STR, key.c_str())){
					// IsFullmap
					string	value = upper(value);
					if(INI_K2HFT_YES1_VAL_STR == value || INI_K2HFT_YES2_VAL_STR == value || INI_K2HFT_ON_VAL_STR == value){
						IsFullmap = true;
					}else if(INI_K2HFT_NO1_VAL_STR == value || INI_K2HFT_NO2_VAL_STR == value || INI_K2HFT_OFF_VAL_STR == value){
						IsFullmap = false;
					}else{
						WAN_K2HFTPRN("keyword(%s)'s value(%s) in main section(%s) is unknown value, so skip it.", key.c_str(), reinterpret_cast<const char*>(yevent.data.scalar.value), K2HFT_MAIN_SEC_STR);
					}

				}else if(0 == strcasecmp(INI_K2HFT_K2HINIT_STR, key.c_str())){
					// IsInitialize
					string	value = upper(value);
					if(INI_K2HFT_YES1_VAL_STR == value || INI_K2HFT_YES2_VAL_STR == value || INI_K2HFT_ON_VAL_STR == value){
						IsInitialize = true;
					}else if(INI_K2HFT_NO1_VAL_STR == value || INI_K2HFT_NO2_VAL_STR == value || INI_K2HFT_OFF_VAL_STR == value){
						IsInitialize = false;
					}else{
						WAN_K2HFTPRN("keyword(%s)'s value(%s) in main section(%s) is unknown value, so skip it.", key.c_str(), reinterpret_cast<const char*>(yevent.data.scalar.value), K2HFT_MAIN_SEC_STR);
					}

				}else if(0 == strcasecmp(INI_K2HFT_K2HMASKBIT_STR, key.c_str())){
					// MaskBitCnt
					MaskBitCnt = atoi(reinterpret_cast<const char*>(yevent.data.scalar.value));

				}else if(0 == strcasecmp(INI_K2HFT_K2HCMASKBIT_STR, key.c_str())){
					// CMaskBitCnt
					CMaskBitCnt = atoi(reinterpret_cast<const char*>(yevent.data.scalar.value));

				}else if(0 == strcasecmp(INI_K2HFT_K2HMAXELE_STR, key.c_str())){
					// MaxElementCnt
					MaxElementCnt = atoi(reinterpret_cast<const char*>(yevent.data.scalar.value));

				}else if(0 == strcasecmp(INI_K2HFT_K2HPAGESIZE_STR, key.c_str())){
					// PageSize
					PageSize = static_cast<size_t>(atoll(reinterpret_cast<const char*>(yevent.data.scalar.value)));

				}else if(0 == strcasecmp(INI_K2HFT_DTORTHREADCNT_STR, key.c_str())){
					// DtorThreadCnt
					int	nTmp = atoi(reinterpret_cast<const char*>(yevent.data.scalar.value));
					if(0 >= nTmp){
						ERR_K2HFTPRN("keyword(%s)'s value(%s) in main section(%s) must be over 0.", key.c_str(), reinterpret_cast<const char*>(yevent.data.scalar.value), K2HFT_MAIN_SEC_STR);
						result = false;
					}else{
						DtorThreadCnt = nTmp;
					}

				}else if(0 == strcasecmp(INI_K2HFT_DTORCTP_STR, key.c_str())){
					// DtorCtpPath
					DtorCtpPath = K2HFT_K2HTPDTOR;
					if(!check_path_real_path(reinterpret_cast<const char*>(yevent.data.scalar.value), DtorCtpPath)){
						ERR_K2HFTPRN("keyword(%s)'s value(%s) in main section(%s) does not exist.", key.c_str(), reinterpret_cast<const char*>(yevent.data.scalar.value), K2HFT_MAIN_SEC_STR);
						result = false;
					}

				}else if(0 == strcasecmp(INI_K2HFT_BINTRANS_STR, key.c_str())){
					// Binary Transfer Mode
					string	value = upper(value);
					if(INI_K2HFT_YES1_VAL_STR == value || INI_K2HFT_YES2_VAL_STR == value || INI_K2HFT_ON_VAL_STR == value){
						IsBinaryMode = true;
					}else if(INI_K2HFT_NO1_VAL_STR == value || INI_K2HFT_NO2_VAL_STR == value || INI_K2HFT_OFF_VAL_STR == value){
						IsBinaryMode = false;
					}else{
						WAN_K2HFTPRN("keyword(%s)'s value(%s) in main section(%s) is unknown value, so skip it.", key.c_str(), reinterpret_cast<const char*>(yevent.data.scalar.value), K2HFT_MAIN_SEC_STR);
					}

				}else if(0 == strcasecmp(INI_K2HFT_EXPIRE_STR, key.c_str())){
					// Expire seconds
					time_t	expire_sec = static_cast<time_t>(atoi(reinterpret_cast<const char*>(yevent.data.scalar.value)));
					if(expire_sec <= 0){
						ERR_K2HFTPRN("keyword(%s)'s value(%s) in main section(%s) must be over 0.", key.c_str(), reinterpret_cast<const char*>(yevent.data.scalar.value), K2HFT_MAIN_SEC_STR);
						result = false;
					}else{
						ExpireTime = expire_sec;
					}

				}else if(0 == strcasecmp(INI_K2HFT_TRANSLINECNT_STR, key.c_str())){
					// Minimum transfer line limit
					int	linecnt = atoi(reinterpret_cast<const char*>(yevent.data.scalar.value));
					if(linecnt < 0){
						ERR_K2HFTPRN("keyword(%s)'s value(%s) in main section(%s) must be a positive number including 0.", key.c_str(), reinterpret_cast<const char*>(yevent.data.scalar.value), K2HFT_MAIN_SEC_STR);
						result = false;
					}else{
						K2hFtWriteBuff::SetStackLineMax(static_cast<size_t>(linecnt));
					}

				}else if(0 == strcasecmp(INI_K2HFT_TRANSTIMEUP_STR, key.c_str())){
					// transfer timeup limit
					int	timeup = atoi(reinterpret_cast<const char*>(yevent.data.scalar.value));
					if(timeup < 0){
						ERR_K2HFTPRN("keyword(%s)'s value(%s) in main section(%s) must be a positive number including 0.", key.c_str(), reinterpret_cast<const char*>(yevent.data.scalar.value), K2HFT_MAIN_SEC_STR);
						result = false;
					}else{
						K2hFtWriteBuff::SetStackTimeup(static_cast<time_t>(timeup));
					}

				}else if(0 == strcasecmp(INI_K2HFT_BYTELIMIT_STR, key.c_str())){
					// transfer timeup limit
					int	limit = atoi(reinterpret_cast<const char*>(yevent.data.scalar.value));
					if(limit < 0){
						ERR_K2HFTPRN("keyword(%s)'s value(%s) in main section(%s) must be a positive number including 0.", key.c_str(), reinterpret_cast<const char*>(yevent.data.scalar.value), K2HFT_MAIN_SEC_STR);
						result = false;
					}else{
						K2hFtWriteBuff::SetLineByteLimit(static_cast<size_t>(limit));
					}

				}else{
					WAN_K2HFTPRN("Found unexpected key(%s) in %s section, thus skip this key and value.", key.c_str(), K2HFT_MAIN_SEC_STR);
				}
				key.clear();
			}
		}else{
			// [TODO] Now not support alias(anchor) event
			//
			ERR_K2HFTPRN("Found unexpected yaml event(%d) in %s section.", yevent.type, K2HFT_MAIN_SEC_STR);
			result = false;
		}

		// delete event
		if(is_loop){
			is_loop = yevent.type != YAML_STREAM_END_EVENT;
		}
		if(need_delete_event){
			yaml_event_delete(&yevent);
		}
	}

	if(result){
		// check values
		if(IsMemType && !K2hFilePath.empty()){
			ERR_K2HFTPRN("Specified memory type k2hash, but k2hash file path(%s) is specified.", K2hFilePath.c_str());
			Clean();
			return false;
		}
		if(K2hFilePath.empty() && !IsInitialize){
			ERR_K2HFTPRN("k2hash type is %s, but specified initialize flag as false.", IsMemType ? "memory" : "temporary file");
			Clean();
			return false;
		}
		if(K2hFilePath.empty() && !IsFullmap){
			ERR_K2HFTPRN("k2hash type is %s, but specified full mapping flag as false.", IsMemType ? "memory" : "temporary file");
			Clean();
			return false;
		}
	}else{
		Clean();
	}
	return result;
}

bool K2hFtInfo::LoadYamlRuleSec(yaml_parser_t& yparser, mode_t init_mode, mode_t init_dmode, uid_t init_uid, gid_t init_gid, K2hFtPluginMan& pluginman, bool is_dir_rule)
{
	// Must start yaml sequence(for mapping array) -> mapping event.
	yaml_event_t	yevent;
	{
		// sequence
		if(!yaml_parser_parse(&yparser, &yevent)){
			ERR_K2HFTPRN("Could not parse event. errno = %d", errno);
			return false;
		}
		if(YAML_SEQUENCE_START_EVENT != yevent.type){
			ERR_K2HFTPRN("Parsed event type is not start sequence(%d)", yevent.type);
			yaml_event_delete(&yevent);
			return false;
		}
		yaml_event_delete(&yevent);
	}

	// RuleMap(Rule or RuleDir)
	PK2HFTRULE			pTmpRule = NULL;
	k2hftmatchlist_t	AllowMatchs;
	k2hftmatchlist_t	DenyMatchs;

	// Loading
	string	key("");
	bool	result				= true;
	bool	need_delete_event	= false;
	for(bool is_loop = true, in_mapping = false; is_loop && result; ){
		// get event
		if(!yaml_parser_parse(&yparser, &yevent)){
			ERR_K2HFTPRN("Could not parse event. errno = %d", errno);
			result = false;
			continue;				// break loop assap.
		}
		need_delete_event = true;

		// check event
		if(YAML_MAPPING_START_EVENT == yevent.type){
			// Start mapping event
			if(in_mapping){
				ERR_K2HFTPRN("Already start yaml mapping event in %s section loop.", (is_dir_rule ? K2HFT_RULEDIR_SEC_STR : K2HFT_RULE_SEC_STR));
				result = false;
			}else{
				in_mapping = true;

				// initialize
				pTmpRule = new K2HFTRULE;
				AllowMatchs.clear();
				DenyMatchs.clear();
			}

		}else if(YAML_MAPPING_END_EVENT == yevent.type){
			// End mapping event
			if(!in_mapping){
				ERR_K2HFTPRN("Already stop yaml mapping event in %s section loop.", (is_dir_rule ? K2HFT_RULEDIR_SEC_STR : K2HFT_RULE_SEC_STR));
				result = false;
			}else{
				// Finish one rule ---> Set rule data
				//
				in_mapping = false;

				// set Matching List
				if(pTmpRule->DefaultDenyAll){
					// check deny list
					if(0 < DenyMatchs.size()){
						WAN_K2HFTPRN("rule section(%s) is Default Deny from All, but it has some DENY keywords, so skip those and continue...", (is_dir_rule ? K2HFT_RULEDIR_SEC_STR : K2HFT_RULE_SEC_STR));

						for(k2hftmatchlist_t::iterator iter = DenyMatchs.begin(); iter != DenyMatchs.end(); ++iter){
							K2HFT_DELETE(*iter);
						}
						DenyMatchs.clear();
					}
					// copy allow list
					for(k2hftmatchlist_t::iterator iter = AllowMatchs.begin(); iter != AllowMatchs.end(); ++iter){
						pTmpRule->MatchingList.push_back(*iter);
					}
					AllowMatchs.clear();

				}else{
					// check allow list
					if(0 < AllowMatchs.size()){
						WAN_K2HFTPRN("rule section(%s) is Default Deny from All, but it has some DENY keywords, so skip those and continue...", (is_dir_rule ? K2HFT_RULEDIR_SEC_STR : K2HFT_RULE_SEC_STR));

						for(k2hftmatchlist_t::iterator iter = AllowMatchs.begin(); iter != AllowMatchs.end(); ++iter){
							K2HFT_DELETE(*iter);
						}
						AllowMatchs.clear();
					}
					// copy deny list
					for(k2hftmatchlist_t::iterator iter = DenyMatchs.begin(); iter != DenyMatchs.end(); ++iter){
						pTmpRule->MatchingList.push_back(*iter);
					}
					DenyMatchs.clear();
				}

				// check plugin and set output file/execute flag
				if(pTmpRule->pPlugin){
					pTmpRule->pPlugin->mode			= is_dir_rule ? init_dmode : init_mode;
					pTmpRule->pPlugin->not_execute	= is_dir_rule;

					// [NOTE]
					// If output path is relative path, convert it to absolute path for plugin here.
					// Original output path will be convert absolute path in InitializeOutputFiles().
					//
					if('/' != pTmpRule->OutputPath[0]){
						// output path is relative, so convert to absolute( mountpoint/... )
						pTmpRule->pPlugin->OutputPath	= K2hFtManage::GetMountPoint();							// mount point is not terminated '/'
						pTmpRule->pPlugin->OutputPath	+= '/';
						pTmpRule->pPlugin->OutputPath	+= pTmpRule->OutputPath;
					}else{
						pTmpRule->pPlugin->OutputPath	= pTmpRule->OutputPath;
					}
				}

				// set other member
				pTmpRule->Mode		= is_dir_rule ? init_dmode : init_mode;
				pTmpRule->Uid		= init_uid;
				pTmpRule->Gid		= init_gid;
				pTmpRule->AccumSize	= is_dir_rule ? static_cast<off_t>(get_system_pagesize()) : 0;
				pTmpRule->StartTime	= InitTime;
				pTmpRule->LastTime	= InitTime;

				// set into RuleMap
				RuleMap[pTmpRule->TargetPath] = pTmpRule;

				// clear
				pTmpRule = NULL;
				AllowMatchs.clear();
				DenyMatchs.clear();
			}

		}else if(YAML_SEQUENCE_START_EVENT == yevent.type){
			ERR_K2HFTPRN("Parsed event type is not start sequence(%d)", yevent.type);
			yaml_event_delete(&yevent);
			Clean();
			return false;

		}else if(YAML_SEQUENCE_END_EVENT == yevent.type){
			// End sequence(for mapping) event
			if(in_mapping){
				ERR_K2HFTPRN("Found yaml end sequence event, but not stop yaml mapping event in %s section loop.", (is_dir_rule ? K2HFT_RULEDIR_SEC_STR : K2HFT_RULE_SEC_STR));
				result = false;
			}else{
				// Finish loop without error.
				//
				is_loop = false;
			}

		}else if(YAML_SCALAR_EVENT == yevent.type){
			// Load key & value
			if(key.empty()){
				key = reinterpret_cast<const char*>(yevent.data.scalar.value);

				// ALLOW / DENY is special
				if(0 == strcasecmp(INI_K2HFT_ALLOW_STR, key.c_str()) || 0 == strcasecmp(INI_K2HFT_DENY_STR, key.c_str())){
					// allow and deny value is not simple, thus call sub method
					yaml_event_delete(&yevent);
					need_delete_event = false;
					key.clear();

					if(!LoadYamlAclPart(yparser, (0 == strcasecmp(INI_K2HFT_ALLOW_STR, key.c_str()) ? AllowMatchs : DenyMatchs))){
						ERR_K2HFTPRN("Found yaml end sequence event, but not stop yaml mapping event in %s section loop.", (is_dir_rule ? K2HFT_RULEDIR_SEC_STR : K2HFT_RULE_SEC_STR));
						result = false;
					}
				}

			}else{
				//
				// Compare key and set value
				//
				if(0 == strcasecmp(INI_K2HFT_TARGET_STR, key.c_str())){
					// K2HFTRULE::TargetPath
					pTmpRule->TargetPath = reinterpret_cast<const char*>(yevent.data.scalar.value);

				}else if(0 == strcasecmp(INI_K2HFT_TRUNS_STR, key.c_str())){
					// K2HFTRULE::IsTransfer
					string	value = upper(reinterpret_cast<const char*>(yevent.data.scalar.value));
					if(INI_K2HFT_YES1_VAL_STR == value || INI_K2HFT_YES2_VAL_STR == value || INI_K2HFT_ON_VAL_STR == value){
						pTmpRule->IsTransfer = true;
					}else if(INI_K2HFT_NO1_VAL_STR == value || INI_K2HFT_NO2_VAL_STR == value || INI_K2HFT_OFF_VAL_STR == value){
						pTmpRule->IsTransfer = false;
					}else{
						WAN_K2HFTPRN("keyword(%s)'s value(%s) in rule section(%s) is unknown value, so skip it.", key.c_str(), reinterpret_cast<const char*>(yevent.data.scalar.value), (is_dir_rule ? K2HFT_RULEDIR_SEC_STR : K2HFT_RULE_SEC_STR));
					}

				}else if(0 == strcasecmp(INI_K2HFT_OUTPUTFILE_STR, key.c_str())){
					// K2HFTRULE::OutputPath
					if(0 == strcmp(reinterpret_cast<const char*>(yevent.data.scalar.value), "/dev/null")){
						pTmpRule->OutputPath = "";			// not put to file
					}else{
						pTmpRule->OutputPath = reinterpret_cast<const char*>(yevent.data.scalar.value);
					}

				}else if(0 == strcasecmp(INI_K2HFT_PLUGIN_STR, key.c_str())){
					// K2HFTRULE::pPlugin
					if(pTmpRule->pPlugin){
						WAN_K2HFTPRN("keyword(%s) in rule section(%s) is already set, then it is over wrote by new value(%s),", key.c_str(), (is_dir_rule ? K2HFT_RULEDIR_SEC_STR : K2HFT_RULE_SEC_STR), reinterpret_cast<const char*>(yevent.data.scalar.value));
						K2HFT_DELETE(pTmpRule->pPlugin);
					}
					pTmpRule->pPlugin			= new K2HFT_PLUGIN;
					pTmpRule->pPlugin->BaseParam= reinterpret_cast<const char*>(yevent.data.scalar.value);

					// add plugin to manager
					if(!pluginman.AddPluginInfo(pTmpRule->pPlugin)){
						ERR_K2HFTPRN("keyword(%s) in rule section(%s) has plugin, but failed to set it to plugin manager.", key.c_str(), (is_dir_rule ? K2HFT_RULEDIR_SEC_STR : K2HFT_RULE_SEC_STR));
						K2HFT_DELETE(pTmpRule->pPlugin);
					}

				}else if(0 == strcasecmp(INI_K2HFT_DEFAULTALL_STR, key.c_str())){
					// K2HFTRULE::DefaultDenyAll
					string	value = upper(reinterpret_cast<const char*>(yevent.data.scalar.value));
					if(INI_K2HFT_DENY_VAL_STR == value){
						pTmpRule->DefaultDenyAll = true;
					}else if(INI_K2HFT_ALLOW_VAL_STR == value){
						pTmpRule->DefaultDenyAll = false;
					}else{
						WAN_K2HFTPRN("keyword(%s)'s value(%s) in rule section(%s) is unknown value, so skip it.", key.c_str(), reinterpret_cast<const char*>(yevent.data.scalar.value), (is_dir_rule ? K2HFT_RULEDIR_SEC_STR : K2HFT_RULE_SEC_STR));
					}
				}else{
					WAN_K2HFTPRN("Found unexpected key(%s) in %s section, thus skip this key and value.", key.c_str(), (is_dir_rule ? K2HFT_RULEDIR_SEC_STR : K2HFT_RULE_SEC_STR));
				}
				key.clear();
			}
		}else{
			// [TODO] Now not support alias(anchor) event
			//
			ERR_K2HFTPRN("Found unexpected yaml event(%d) in %s section.", yevent.type, K2HFT_MAIN_SEC_STR);
			result = false;
		}

		// delete event
		if(is_loop){
			is_loop = yevent.type != YAML_STREAM_END_EVENT;
		}
		if(need_delete_event){
			yaml_event_delete(&yevent);
		}
	}

	if(result){
		// check and prepare for output files
		if(!InitializeOutputFiles()){
			ERR_K2HFTPRN("Could not initialize(make) some rule output file or directory.");
			Clean();
		}else{
			// check binary mode
			CheckBinaryMode();
		}
	}else{
		Clean();
	}
	return result;
}

bool K2hFtInfo::LoadYamlAclPart(yaml_parser_t& yparser, k2hftmatchlist_t& Matchs)
{
	// ALLOW / DENY value is YAML_SCALAR_EVENT or YAML_SEQUENCE_START_EVENT
	//
	bool	result = true;
	for(bool is_loop = true, in_parent_sequence = false; is_loop && result; ){
		yaml_event_t	yevent;
		if(!yaml_parser_parse(&yparser, &yevent)){
			ERR_K2HFTPRN("Could not parse event. errno = %d", errno);
			result = false;
			continue;
		}

		if(YAML_SCALAR_EVENT == yevent.type){
			// value is not array value
			//
			PK2HFTMATCHPTN	pMatchPtn	= NULL;
			if(K2HFT_ISEMPTYSTR(reinterpret_cast<const char*>(yevent.data.scalar.value))){
				// there is no value ---> any matching
				MSG_K2HFTPRN("allow(or deny)'s value in rule has nothing, this is any pattern hits.");
				pMatchPtn			= new K2HFTMATCHPTN;
				pMatchPtn->IsRegex 	= false;
			}else{
				pMatchPtn			= build_k2hft_match_pattern(reinterpret_cast<const char*>(yevent.data.scalar.value), "");
			}
			if(!pMatchPtn){
				WAN_K2HFTPRN("allow(or deny)'s value in rule is something wrong, so skip it.");
			}else{
				Matchs.push_back(pMatchPtn);
			}
			yaml_event_delete(&yevent);

			if(!in_parent_sequence){
				// This case is only one value by scalar
				is_loop = false;
			}

		}else if(YAML_SEQUENCE_START_EVENT == yevent.type){
			if(!in_parent_sequence){
				// allow/deny value is array.
				yaml_event_delete(&yevent);
				in_parent_sequence = true;

			}else{
				// load array value
				yaml_event_delete(&yevent);

				if(!LoadYamlAclOneValue(yparser, Matchs)){
					ERR_K2HFTPRN("allow(or deny)'s value in rule is something wrong.");
					result = false;
				}else{
					// check end sequence for array value
					if(!yaml_parser_parse(&yparser, &yevent)){
						ERR_K2HFTPRN("Could not parse event. errno = %d", errno);
						result = false;
					}else{
						if(YAML_SEQUENCE_END_EVENT != yevent.type){
							ERR_K2HFTPRN("Could not find end of sequence for allow(or deny) one value.");
							result = false;
						}
						yaml_event_delete(&yevent);
					}
				}
			}

		}else if(YAML_SEQUENCE_END_EVENT == yevent.type){
			if(!in_parent_sequence){
				ERR_K2HFTPRN("Found sequence end, but now in sequence.");
				result = false;
			}else{
				// finish
				MSG_K2HFTPRN("Found sequence end for allow/deny values array.");
				in_parent_sequence	= false;
				is_loop				= false;
			}
			yaml_event_delete(&yevent);

		}else{
			ERR_K2HFTPRN("Parsed event type for allow/deny is not start sequence or scalar(%d)", yevent.type);
			yaml_event_delete(&yevent);
			result = false;
		}
	}
	return result;
}

bool K2hFtInfo::LoadYamlAclOneValue(yaml_parser_t& yparser, k2hftmatchlist_t& Matchs)
{
	// Load one array value in YAML_SEQUENCE_START_EVENT(ALLOW / DENY array)
	//
	// get first value
	yaml_event_t	yevent;
	if(!yaml_parser_parse(&yparser, &yevent)){
		ERR_K2HFTPRN("Could not parse event. errno = %d", errno);
		return false;
	}
	if(YAML_SCALAR_EVENT != yevent.type){
		ERR_K2HFTPRN("allow(or deny)'s first value type(%d) is ignore.", yevent.type);
		yaml_event_delete(&yevent);
		return false;
	}
	string	firstvalue = (!K2HFT_ISEMPTYSTR(reinterpret_cast<const char*>(yevent.data.scalar.value)) ? reinterpret_cast<const char*>(yevent.data.scalar.value) : "");
	yaml_event_delete(&yevent);

	// get second value
	if(!yaml_parser_parse(&yparser, &yevent)){
		ERR_K2HFTPRN("Could not parse event. errno = %d", errno);
		return false;
	}
	if(YAML_SCALAR_EVENT != yevent.type){
		ERR_K2HFTPRN("allow(or deny)'s second value type(%d) is ignore.", yevent.type);
		yaml_event_delete(&yevent);
		return false;
	}
	string	secondvalue = (!K2HFT_ISEMPTYSTR(reinterpret_cast<const char*>(yevent.data.scalar.value)) ? reinterpret_cast<const char*>(yevent.data.scalar.value) : "");
	yaml_event_delete(&yevent);

	// set
	PK2HFTMATCHPTN	pMatchPtn = NULL;
	if(firstvalue.empty()){
		// there is empty first value ---> any matching
		MSG_K2HFTPRN("allow(or deny)'s first value in rule has nothing, this is any pattern hits.");
		pMatchPtn			= new K2HFTMATCHPTN;
		pMatchPtn->IsRegex 	= false;
	}else{
		pMatchPtn			= build_k2hft_match_pattern(firstvalue.c_str(), secondvalue.c_str());
	}
	if(!pMatchPtn){
		WAN_K2HFTPRN("allow(or deny)'s value in rule is something wrong, so skip it.");
	}else{
		Matchs.push_back(pMatchPtn);
	}
	return true;
}

bool K2hFtInfo::Dump(void) const
{
	if(Config.empty()){
		ERR_K2HFTPRN("Does not load configuration file yet.");
		return false;
	}

	int		count;
	int		mcount;
	int		rcount;
	k2hftrulemap_t::const_iterator		iter;
	k2hftmatchlist_t::const_iterator	miter;
	k2hftreplist_t::const_iterator		riter;

	string	strtype;
	if(IsMemType){
		strtype = "memory";
	}else if(K2hFilePath.empty()){
		strtype = "temporary file";
	}else{
		strtype = "file type(";
		strtype += K2hFilePath;
		strtype += ")";
	}

	// common
	K2HFTPRN("Configuration(file or json): %s",	Config.c_str());
	K2HFTPRN("K2hash = {");
	K2HFTPRN("  type                     : %s",	strtype.c_str());
	K2HFTPRN("  Initialize file(memory)  : %s", IsInitialize ? "yes" : "no");
	K2HFTPRN("  mapping mode             : %s", IsFullmap ? "full mapping" : "not full mapping");
	K2HFTPRN("  mask                     : %d",	MaskBitCnt);
	K2HFTPRN("  collision mask           : %d",	CMaskBitCnt);
	K2HFTPRN("  max element count        : %d",	MaxElementCnt);
	K2HFTPRN("  page size                : %zu byte",	PageSize);
	K2HFTPRN("}");
	K2HFTPRN("Other option = {");
	K2HFTPRN("  h2tdtor thread count     : %d",	DtorThreadCnt);
	K2HFTPRN("  h2tdtor plugin           : %s", DtorCtpPath.c_str());
	K2HFTPRN("  binary transfer mode     : %s",	IsBinaryMode ? "yes" : "no");
	K2HFTPRN("  expire time for transfer : %zds(0 means no expire)", ExpireTime);
	K2HFTPRN("}");

	// Lock
	while(!fullock::flck_trylock_noshared_mutex(prule_lockval));

	// rules
	for(count = 1, iter = RuleMap.begin(); iter != RuleMap.end(); ++iter){
		K2HFTPRN("Rules[%d] = {", count);
		K2HFTPRN("  Target Path              : %s", 		iter->second->TargetPath.c_str());
		K2HFTPRN("  Transfer                 : %s", 		iter->second->IsTransfer ? "yes" : "no");
		K2HFTPRN("  Output file(directory)   : %s", 		iter->second->OutputPath.empty() ? "null(/dev/null)" : iter->second->OutputPath.c_str());
		K2HFTPRN("  Plugin                   : %s", 		iter->second->pPlugin ? iter->second->pPlugin->BaseParam.c_str() : "(no plugin)");
		K2HFTPRN("  Default                  : %s", 		iter->second->DefaultDenyAll ? "deny from all" : "allow from all");
		K2HFTPRN("  mode                     : %07o(%s)", 	iter->second->Mode, S_ISREG(iter->second->Mode) ? "regular file" : S_ISDIR(iter->second->Mode) ? "directory" : "unknown");
		K2HFTPRN("  uid                      : %d", 		iter->second->Uid);
		K2HFTPRN("  gid                      : %d", 		iter->second->Gid);
		K2HFTPRN("  size                     : %zd", 		iter->second->AccumSize);
		K2HFTPRN("  start time               : %zus %ldns", iter->second->StartTime.tv_sec, iter->second->StartTime.tv_nsec);
		K2HFTPRN("  last time                : %zus %ldns", iter->second->LastTime.tv_sec, iter->second->LastTime.tv_nsec);
		K2HFTPRN("  Matching                 : %d count",	iter->second->MatchingList.size());
		for(mcount = 1, miter = iter->second->MatchingList.begin(); miter != iter->second->MatchingList.end(); ++mcount, ++miter){
			K2HFTPRN("    [%d] = {",	mcount);
			K2HFTPRN("      Regex                : %s",	(*miter)->IsRegex ? "yes" : "no");
			K2HFTPRN("      Base String          : %s",	(*miter)->BaseString.c_str());
			K2HFTPRN("      Replace String = {");
			for(rcount = 1, riter = (*miter)->ReplaceList.begin(); riter != (*miter)->ReplaceList.end(); ++rcount, ++riter){
				string	tmprep;
				if(-1 == (*riter)->matchno){
					tmprep	= (*riter)->substring;
				}else{
					tmprep	= "$";
					tmprep	+= to_string((*riter)->matchno);
				}
				K2HFTPRN("        [%d]               : %s", rcount, tmprep.c_str());
			}
			K2HFTPRN("      }");
			K2HFTPRN("    }");
		}
		K2HFTPRN("  }");
		K2HFTPRN("}");
		++count;
	}

	// Unlock
	fullock::flck_unlock_noshared_mutex(prule_lockval);

	return true;
}

bool K2hFtInfo::FindPath(const char* path, struct stat& stbuf) const
{
	if(K2HFT_ISEMPTYSTR(path)){
		ERR_K2HFTPRN("path is empty.");
		return false;
	}
	memset(&stbuf, 0, sizeof(stbuf));

	if(0 == strcmp(path, "/")){
		// mount point
		stbuf.st_mode		= DirMode;
		stbuf.st_nlink		= 2;								// always 2 for directory(see FUSE FAQ)
		stbuf.st_uid		= DirUid;
		stbuf.st_gid		= DirGid;
		stbuf.st_size		= static_cast<off_t>(get_system_pagesize());
		stbuf.st_blksize	= get_system_pagesize();			// pagesize(but this field is ignored by fuse)
		stbuf.st_blocks		= GET_STAT_BLOCKS(stbuf.st_size);	// 512 byte is 1 block
		stbuf.st_atim		= InitTime;
		stbuf.st_mtim		= InitTime;
		stbuf.st_ctim		= InitTime;
		return true;
	}
	if('/' == *path){
		++path;				// skip first '/'
	}

	if(!IsLoad()){
		ERR_K2HFTPRN("Not load configuration file.");
		return false;
	}

	// Lock
	while(!fullock::flck_trylock_noshared_mutex(prule_lockval));

	for(k2hftrulemap_t::const_iterator iter = RuleMap.begin(); iter != RuleMap.end(); ++iter){
		if(0 != (iter->first).find(path)){
			// must match at first position.
			continue;
		}
		if((iter->first).length() == strlen(path)){
			// found completely same path
			stbuf.st_mode		= iter->second->Mode;
			stbuf.st_nlink		= S_ISDIR(iter->second->Mode) ? 2 :1;	// see FUSE FAQ
			stbuf.st_uid		= iter->second->Uid;
			stbuf.st_gid		= iter->second->Gid;
			stbuf.st_size		= iter->second->AccumSize;
			stbuf.st_blksize	= get_system_pagesize();			// pagesize(but this field is ignored by fuse)
			stbuf.st_blocks		= GET_STAT_BLOCKS(stbuf.st_size);	// 512 byte is 1 block
			stbuf.st_atim		= iter->second->LastTime;
			stbuf.st_mtim		= iter->second->LastTime;
			stbuf.st_ctim		= iter->second->StartTime;

			fullock::flck_unlock_noshared_mutex(prule_lockval);		// Unlock
			return true;
		}
		if('/' == (iter->first)[strlen(path)]){
			// found directory path
			stbuf.st_mode		= DirMode;
			stbuf.st_nlink		= 2;								// always 2 for directory(see FUSE FAQ)
			stbuf.st_uid		= DirUid;
			stbuf.st_gid		= DirGid;
			stbuf.st_size		= static_cast<off_t>(get_system_pagesize());
			stbuf.st_blksize	= get_system_pagesize();			// pagesize(but this field is ignored by fuse)
			stbuf.st_blocks		= GET_STAT_BLOCKS(stbuf.st_size);	// 512 byte is 1 block
			stbuf.st_atim		= InitTime;
			stbuf.st_mtim		= InitTime;
			stbuf.st_ctim		= InitTime;

			fullock::flck_unlock_noshared_mutex(prule_lockval);		// Unlock
			return true;
		}
		// found same path, but it is part of file or directory name, so skip it.
	}
	// Unlock
	fullock::flck_unlock_noshared_mutex(prule_lockval);

	// Not found the path.
	return false;
}

uint64_t K2hFtInfo::GetFileHandle(const char* path) const
{
	if(K2HFT_ISEMPTYSTR(path)){
		ERR_K2HFTPRN("path is empty.");
		return 0;
	}
	if(0 == strcmp(path, "/")){
		return 0;			// directory
	}
	if('/' == *path){
		++path;				// skip first '/'
	}
	if(!IsLoad()){
		ERR_K2HFTPRN("Not load configuration file.");
		return 0;
	}

	// Lock
	while(!fullock::flck_trylock_noshared_mutex(prule_lockval));

	for(k2hftrulemap_t::const_iterator iter = RuleMap.begin(); iter != RuleMap.end(); ++iter){
		if(0 != (iter->first).find(path)){
			// must match at first position.
			continue;
		}
		if((iter->first).length() == strlen(path)){
			// found completely same path
			fullock::flck_unlock_noshared_mutex(prule_lockval);		// Unlock
			return reinterpret_cast<uint64_t>(iter->second);
		}
	}
	// Unlock
	fullock::flck_unlock_noshared_mutex(prule_lockval);

	// Not found the path.
	return 0;
}

bool K2hFtInfo::ReadDir(const char* path, k2hftldlist_t& list) const
{
	if(K2HFT_ISEMPTYSTR(path)){
		ERR_K2HFTPRN("path is empty.");
		return false;
	}
	if('/' == *path){
		++path;				// skip first '/'
	}
	list.clear();

	PK2HFTLISTDIR	ptempld = NULL;
	{
		// "."
		ptempld			= new K2HFTLISTDIR;
		ptempld->strname= ".";
		ptempld->pstat	= NULL;
		list.push_back(ptempld);

		// ".."
		ptempld			= new K2HFTLISTDIR;
		ptempld->strname= "..";
		ptempld->pstat	= NULL;
		list.push_back(ptempld);
	}

	// Lock
	while(!fullock::flck_trylock_noshared_mutex(prule_lockval));

	for(k2hftrulemap_t::const_iterator iter = RuleMap.begin(); iter != RuleMap.end(); ++iter){
		// make target base name in directory(path)
		string				basename;
		string::size_type	pos;
		bool				is_middle_dir = false;				// this flag means "case of detects an intermediate path"
		if(K2HFT_ISEMPTYSTR(path)){
			basename = (iter->first);							// "xxx/yyy/..." --> "xxx/yyy..."
			if(string::npos != (pos = basename.find("/"))){
				is_middle_dir	= true;
				basename		= basename.substr(0, pos);
			}
		}else{
			if(0 != (iter->first).find(path)){
				// must match at first position.
				continue;
			}
			if((iter->first).length() == strlen(path)){
				// found completely same path -> it's me
				continue;
			}
			if('/' != (iter->first)[strlen(path)]){
				// found same path, but it is part of file or directory name, so skip it.
				continue;
			}

			// found directory path
			basename = (iter->first).substr(strlen(path) + 1);	// "path/xxx/yyy/..." --> "xxx/yyy..."
			if(string::npos != (pos = basename.find("/"))){
				is_middle_dir	= true;
				basename		= basename.substr(0, pos);
			}
		}
		if(basename.empty()){
			// why?
			continue;
		}

		// check same basename
		bool	already_set = false;
		for(k2hftldlist_t::const_iterator liter = list.begin(); liter != list.end(); ++liter){
			if((*liter)->strname == basename){
				already_set = true;
				break;
			}
		}
		if(already_set){
			continue;
		}

		// set
		ptempld						= new K2HFTLISTDIR;
		ptempld->strname			= basename;
		ptempld->pstat				= new (struct stat);

		memset(ptempld->pstat, 0, sizeof(struct stat));
		ptempld->pstat->st_mode		= is_middle_dir ? DirMode : iter->second->Mode;
		ptempld->pstat->st_nlink	= is_middle_dir ? 2 : (S_ISDIR(iter->second->Mode) ? 2 : 1);
		ptempld->pstat->st_uid		= is_middle_dir ? DirUid : iter->second->Uid;
		ptempld->pstat->st_gid		= is_middle_dir ? DirGid : iter->second->Gid;
		ptempld->pstat->st_size		= is_middle_dir ? static_cast<off_t>(get_system_pagesize()) : iter->second->AccumSize;
		ptempld->pstat->st_blksize	= get_system_pagesize();
		ptempld->pstat->st_blocks	= GET_STAT_BLOCKS(ptempld->pstat->st_size);
		ptempld->pstat->st_atim		= is_middle_dir ? InitTime : iter->second->LastTime;
		ptempld->pstat->st_mtim		= is_middle_dir ? InitTime : iter->second->LastTime;
		ptempld->pstat->st_ctim		= is_middle_dir ? InitTime : iter->second->StartTime;

		list.push_back(ptempld);
	}
	// Unlock
	fullock::flck_unlock_noshared_mutex(prule_lockval);

	return true;
}

bool K2hFtInfo::TruncateZero(const char* path)
{
	if(K2HFT_ISEMPTYSTR(path)){
		ERR_K2HFTPRN("path is empty.");
		return false;
	}
	if(0 == strcmp(path, "/")){
		ERR_K2HFTPRN("path is root directory.");
		return false;
	}
	if('/' == *path){
		++path;				// skip first '/'
	}
	if(!IsLoad()){
		ERR_K2HFTPRN("Not load configuration file.");
		return false;
	}

	// Lock
	while(!fullock::flck_trylock_noshared_mutex(prule_lockval));

	for(k2hftrulemap_t::const_iterator iter = RuleMap.begin(); iter != RuleMap.end(); ++iter){
		if((iter->first) == path){
			// found target path.
			bool	result = true;
			if(S_ISDIR(iter->second->Mode)){
				ERR_K2HFTPRN("%s is directory.", path);
				result = false;
			}else{
				iter->second->AccumSize = 0;
				if(!set_now_timespec(iter->second->LastTime)){
					ERR_K2HFTPRN("Could not get now timespec for %s", path);
					result = false;
				}
			}
			fullock::flck_unlock_noshared_mutex(prule_lockval);		// Unlock
			return result;
		}
	}
	// Unlock
	fullock::flck_unlock_noshared_mutex(prule_lockval);

	// Not found the path.
	return false;
}

bool K2hFtInfo::SetOwner(const char* path, uid_t uid, gid_t gid)
{
	if(K2HFT_ISEMPTYSTR(path)){
		ERR_K2HFTPRN("path is empty.");
		return false;
	}
	if(0 == strcmp(path, "/")){
		ERR_K2HFTPRN("path is root directory.");
		return false;
	}
	if('/' == *path){
		++path;				// skip first '/'
	}
	if(!IsLoad()){
		ERR_K2HFTPRN("Not load configuration file.");
		return false;
	}

	// Lock
	while(!fullock::flck_trylock_noshared_mutex(prule_lockval));

	for(k2hftrulemap_t::const_iterator iter = RuleMap.begin(); iter != RuleMap.end(); ++iter){
		if((iter->first) == path){
			// found target path.
			bool	result = true;
			if(S_ISDIR(iter->second->Mode)){
				ERR_K2HFTPRN("%s is directory.", path);
				result = false;
			}else{
				if(uid != static_cast<uid_t>(-1)){
					iter->second->Uid = uid;
				}
				if(gid != static_cast<gid_t>(-1)){
					iter->second->Gid = gid;
				}
				if(!set_now_timespec(iter->second->LastTime)){
					ERR_K2HFTPRN("Could not get now timespec for %s", path);
					result = false;
				}
			}
			fullock::flck_unlock_noshared_mutex(prule_lockval);		// Unlock
			return result;
		}
	}
	// Unlock
	fullock::flck_unlock_noshared_mutex(prule_lockval);

	// Not found the path.
	return false;
}

bool K2hFtInfo::SetMode(const char* path, mode_t mode)
{
	if(K2HFT_ISEMPTYSTR(path)){
		ERR_K2HFTPRN("path is empty.");
		return false;
	}
	if(0 == strcmp(path, "/")){
		ERR_K2HFTPRN("path is root directory.");
		return false;
	}
	if('/' == *path){
		++path;				// skip first '/'
	}
	if(!IsLoad()){
		ERR_K2HFTPRN("Not load configuration file.");
		return false;
	}

	// Lock
	while(!fullock::flck_trylock_noshared_mutex(prule_lockval));

	for(k2hftrulemap_t::const_iterator iter = RuleMap.begin(); iter != RuleMap.end(); ++iter){
		if((iter->first) == path){
			// found target path.
			bool	result = true;
			if(S_ISDIR(iter->second->Mode)){
				ERR_K2HFTPRN("%s is directory.", path);
				result = false;
			}else{
				iter->second->Mode &= ~(S_IRWXU | S_IRWXG | S_IRWXO);
				iter->second->Mode |= (mode & (S_IRWXU | S_IRWXG | S_IRWXO));

				if(!set_now_timespec(iter->second->LastTime)){
					ERR_K2HFTPRN("Could not get now timespec for %s", path);
					result = false;
				}
			}
			fullock::flck_unlock_noshared_mutex(prule_lockval);		// Unlock
			return result;
		}
	}
	// Unlock
	fullock::flck_unlock_noshared_mutex(prule_lockval);

	// Not found the path.
	return false;
}

bool K2hFtInfo::SetTimespec(const char* path, const struct timespec& ts)
{
	if(K2HFT_ISEMPTYSTR(path)){
		ERR_K2HFTPRN("path is empty.");
		return false;
	}
	if(0 == strcmp(path, "/")){
		ERR_K2HFTPRN("path is root directory.");
		return false;
	}
	if('/' == *path){
		++path;				// skip first '/'
	}
	if(!IsLoad()){
		ERR_K2HFTPRN("Not load configuration file.");
		return false;
	}

	// [NOTE]
	// Do not lock here.
	//
	for(k2hftrulemap_t::const_iterator iter = RuleMap.begin(); iter != RuleMap.end(); ++iter){
		if((iter->first) == path){
			// found target path.
			bool	result = true;
			if(S_ISDIR(iter->second->Mode)){
				ERR_K2HFTPRN("%s is directory.", path);
				result = false;
			}else{
				iter->second->LastTime = ts;
			}
			return result;
		}
	}
	// Not found the path.
	return false;
}

bool K2hFtInfo::CreateFile(const char* path, mode_t mode, uid_t uid, gid_t gid, uint64_t& handle, K2hFtPluginMan& pluginman)
{
	if(K2HFT_ISEMPTYSTR(path)){
		ERR_K2HFTPRN("path is empty.");
		return false;
	}
	if(0 == strcmp(path, "/")){
		ERR_K2HFTPRN("path is root directory.");
		return false;
	}
	if('/' == *path){
		++path;				// skip first '/'
	}
	if(!IsLoad()){
		ERR_K2HFTPRN("Not load configuration file.");
		return false;
	}

	//
	// First : Check same file in Rule
	//
	// Lock
	while(!fullock::flck_trylock_noshared_mutex(prule_lockval));

	for(k2hftrulemap_t::const_iterator iter = RuleMap.begin(); iter != RuleMap.end(); ++iter){
		if((iter->first) == path){
			// found, nothing to create directory.
			fullock::flck_unlock_noshared_mutex(prule_lockval);		// Unlock
			return true;
		}
	}

	//
	// Second : Check same parent directory in Rule
	//
	// check parent path
	string	parentpath;
	if(!get_parent_dirpath(path, parentpath)){
		ERR_K2HFTPRN("Could not get parent directory path from path(%s).", path);
		return false;
	}

	// set Rule member
	PK2HFTRULE	pTmpRule	= new K2HFTRULE;
	pTmpRule->TargetPath	= path;
	pTmpRule->Mode			= (mode & (S_IRWXU | S_IRWXG | S_IRWXO)) | S_IFREG;
	pTmpRule->Uid			= uid;
	pTmpRule->Gid			= gid;
	pTmpRule->AccumSize		= 0;

	if(!set_now_timespec(pTmpRule->StartTime)){
		pTmpRule->StartTime	= common_init_time;
	}
	pTmpRule->LastTime		= pTmpRule->StartTime;

	// make other Rule, copy from parent
	for(k2hftrulemap_t::const_iterator iter = RuleMap.begin(); iter != RuleMap.end(); ++iter){
		if((iter->first) == parentpath){
			// found target path.
			bool	result = true;
			if(!S_ISDIR(iter->second->Mode)){
				ERR_K2HFTPRN("%s is not directory.", parentpath.c_str());
				K2HFT_DELETE(pTmpRule);
				result = false;
			}else{
				// copy
				pTmpRule->IsTransfer	= iter->second->IsTransfer;
				pTmpRule->DefaultDenyAll= iter->second->DefaultDenyAll;
				pTmpRule->OutputPath	= iter->second->OutputPath;

				if(!pTmpRule->OutputPath.empty() && '/' == pTmpRule->OutputPath[pTmpRule->OutputPath.length() - 1]){
					// case: output path is directory
					string	fname;
					if(!get_file_name(path, fname) && fname.empty()){
						ERR_K2HFTPRN("could not get file name from path(%s).", path);
						K2HFT_DELETE(pTmpRule);
						result = false;
					}else{
						pTmpRule->OutputPath += fname;
					}
				}

				// copy rules
				if(result){
					if(!duplicate_match_list(iter->second->MatchingList, pTmpRule->MatchingList)){
						ERR_K2HFTPRN("Internal error: something error occurred during copy rule.");
						K2HFT_DELETE(pTmpRule);
						result = false;
					}else{
						// add to rulemap
						RuleMap[pTmpRule->TargetPath] = pTmpRule;
						handle = reinterpret_cast<uint64_t>(pTmpRule);
					}
				}

				// copy plugin information & run plugin
				if(iter->second->pPlugin){
					pTmpRule->pPlugin				= new K2HFT_PLUGIN;
					pTmpRule->pPlugin->BaseParam	= iter->second->pPlugin->BaseParam;
					pTmpRule->pPlugin->OutputPath	= pTmpRule->OutputPath;
					pTmpRule->pPlugin->mode			= (pTmpRule->Mode & ~S_IFDIR & ~S_IXUSR & ~S_IXGRP & ~S_IXOTH) | S_IFREG;
					pTmpRule->pPlugin->not_execute	= false;

					// set named pipe file path
					if(!K2hFtPluginMan::BuildNamedPipeFile(pTmpRule->pPlugin->PipeFilePath)){
						ERR_K2HFTPRN("Failed to make named pipe file.");
						K2HFT_DELETE(pTmpRule->pPlugin);
						K2HFT_DELETE(pTmpRule);
						result = false;
					}

					// add & run new plugin
					if(result && !pluginman.RunAddPlugin(pTmpRule->pPlugin)){
						ERR_K2HFTPRN("Internal error: something error occurred during run plugin.");
						K2HFT_DELETE(pTmpRule->pPlugin);
						K2HFT_DELETE(pTmpRule);
						result = false;
					}
				}else{
					pTmpRule->pPlugin				= NULL;
				}
			}
			fullock::flck_unlock_noshared_mutex(prule_lockval);		// Unlock
			return result;
		}
	}
	// Unlock
	fullock::flck_unlock_noshared_mutex(prule_lockval);

	return false;
}

bool K2hFtInfo::UpdateMTime(uint64_t handle)
{
	// [NOTE]
	// Do not lock here.
	//
	PK2HFTRULE	pTmpRule = reinterpret_cast<PK2HFTRULE>(handle);
	if(0 == handle || !pTmpRule){
		MSG_K2HFTPRN("file handle is NULL.");
		return false;
	}
	return set_now_timespec(pTmpRule->LastTime);
}

// [NOTE]
// If returned pointer is not same data pointer, MUST free returned pointer.
// It is allocated in convert_to_output().
//
unsigned char* K2hFtInfo::CvtPushData(const unsigned char* data, size_t length, uint64_t filehandle, size_t& cvtlength) const
{
	if(!data || 0 == length || 0 == filehandle){
		ERR_K2HFTPRN("Parameters are wrong.");
		return NULL;
	}
	// get Rule
	PK2HFTRULE	pRule = reinterpret_cast<PK2HFTRULE>(filehandle);
	if(!pRule){
		ERR_K2HFTPRN("file handle is NULL.");
		return NULL;
	}

	// Make output data
	//
	// [NOTE]
	// If binary mode, all rule does not have MatchingList.
	// If not binary mode, get rsize is length of including '\0' last char length.
	//
	unsigned char*	pOutput;
	cvtlength = 0;
	if(NULL == (pOutput = convert_to_output(pRule, data, length, cvtlength))){
		//MSG_K2HFTPRN("Nothing to output, because something error occurred OR nothing to do.");
		return NULL;
	}
	return pOutput;
}

bool K2hFtInfo::Processing(K2HShm* pk2hash, PK2HFTVALUE pValue, uint64_t filehandle, K2hFtFdCache& fdcache, K2hFtPluginMan& pluginman)
{
	if(!pk2hash || !pValue || 0 == filehandle){
		ERR_K2HFTPRN("Parameters are wrong.");
		return false;
	}

	// get Rule
	PK2HFTRULE	pRule = reinterpret_cast<PK2HFTRULE>(filehandle);
	if(!pRule){
		ERR_K2HFTPRN("file handle is NULL.");
		return false;
	}

	// check
	if(!pRule->IsTransfer && !pRule->pPlugin && pRule->OutputPath.empty()){
		MSG_K2HFTPRN("Nothing to do because not transfer and not put file.");
		return true;
	}

	bool	bresult	= true;

	// trans k2hash
	if(pRule->IsTransfer){
		// Make key
		unsigned char*	pKey		= NULL;
		size_t			keylength	= 0;
		if(NULL == (pKey = BuildK2hFtKey(keylength, pRule->TargetPath.c_str(), getpid()))){
			ERR_K2HFTPRN("Something error occurred during building key, skip it.");
			bresult = false;
		}else{
			// Write k2hash
			if(!pk2hash->Set(pKey, keylength, GetK2hFtValuePtr(pValue), GetK2hFtValueSize(pValue), NULL, true)){
				ERR_K2HFTPRN("Something error occurred during set data into k2hash.");
				bresult = false;
			}
		}
		FreeK2hFtKey(pKey);
	}

	// check plugin/put file
	if(pRule->pPlugin || !pRule->OutputPath.empty()){
		size_t		count	= 0;
		PK2HFTLINE	pLines	= GetK2hFtLines(pValue, &count);

		// loop for line count
		for(size_t cnt = 0; cnt < count && pLines; ++cnt){
			PK2HFTLINE	pNextLines = GetNextK2hFtLine(pLines);

			if(0 < pLines->head.linelength){
				size_t			linelen	= pLines->head.linelength;
				unsigned char*	pOutput	= &(pLines->byLine[K2HFT_LINE_BODY_START_POS]);

				if(pRule->pPlugin){
					// plugin
					if(!pluginman.Write(pRule->pPlugin, pOutput, ((!IsBinaryMode && '\0' == pOutput[linelen - 1]) ? (linelen - 1) : linelen))){
						WAN_K2HFTPRN("Could not write to plugin input pipe.");
						bresult = false;
					}
				}else{
					// put file
					if(!fdcache.Write(pRule->OutputPath, O_WRONLY | O_APPEND | O_CREAT, pRule->Mode, pOutput, ((!IsBinaryMode && '\0' == pOutput[linelen - 1]) ? (linelen - 1) : linelen))){
						ERR_K2HFTPRN("Could not write to output file(%s).", pRule->OutputPath.c_str());
						bresult = false;
					}
				}
			}
			pLines = pNextLines;
		}
	}

	// Update mtime & size
	//
	// [NOTE]
	// Do not lock here.
	//
	{
		// [NOTE]
		// AccumSize is not original data size, it is converted output data size and do not care for null byte at not binary mode.
		// It should be original size with be care for binary mode, but now logic does not support it.
		// Thus AccumSize means transferred byte data.
		//
		pRule->AccumSize += GetK2hFtValueSize(pValue);
		if(!set_now_timespec(pRule->LastTime)){
			WAN_K2HFTPRN("Failed to update last update time(mtime/atime), but continue...");
		}
	}
	return bresult;
}

/*
 * VIM modelines
 *
 * vim:set ts=4 fenc=utf-8:
 */
