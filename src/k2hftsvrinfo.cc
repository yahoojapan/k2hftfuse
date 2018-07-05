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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <chmpx/chmconfutil.h>

#include "k2hftcommon.h"
#include "k2hftsvrinfo.h"
#include "k2hftiniparser.h"
#include "k2hftutil.h"
#include "k2hftdbg.h"

using namespace std;

//---------------------------------------------------------
// Symbols
//---------------------------------------------------------
#define	K2HFTSVR_TIME_NS_PRNFORMAT					"%09ld"		// 999999999ns is max
#define	K2HFTSVR_TIME_NS_KEYWORD					"%-"

//---------------------------------------------------------
// Utilities
//---------------------------------------------------------
inline string get_string_content(const char* value)
{
	string	strtmp("");
	if(!K2HFT_ISEMPTYSTR(value)){
		if('\"' == value[0]){
			char*	ptmp = strdup(++value);
			if(ptmp){
				char*	found = strrchr(ptmp, '\"');
				if(!found){
					WAN_K2HFTPRN("%s string which starts '\"', but it is not terminated '\"'.", value);
				}else{
					*found = '\0';
				}
				strtmp = ptmp;
				K2HFT_FREE(ptmp);
			}
		}else{
			strtmp = value;
		}
	}
	return strtmp;
}

inline void build_time_part_list(const char* form, k2hftsvrtplist_t& list)
{
	list.clear();

	if(K2HFT_ISEMPTYSTR(form)){
		MSG_K2HFTPRN("form is empty.");
	}else{
		K2HFTSVRTIMEPART	data;
		for(string	strform = get_string_content(form); !strform.empty(); ){
			string::size_type	pos = strform.find(K2HFTSVR_TIME_NS_KEYWORD);
			if(string::npos == pos){
				if(!strform.empty()){
					data.is_type_ns	= false;
					data.form		= strform;
					list.push_back(data);
				}
				break;

			}else{
				data.is_type_ns	= false;
				data.form		= strform.substr(0, pos);
				if(!data.form.empty()){
					list.push_back(data);
				}

				data.is_type_ns	= true;
				data.form		= "";
				list.push_back(data);

				strform = strform.substr(pos + strlen(K2HFTSVR_TIME_NS_KEYWORD));
			}
		}
	}
}

inline bool make_time_output_string(string& strtime, const k2hftsvrtplist_t& list, const struct timespec* ts)
{
	if(!ts){
		ERR_K2HFTPRN("ts is empty.");
		return false;
	}
	struct tm*	tm = localtime(&(ts->tv_sec));
	if(!tm){
		ERR_K2HFTPRN("Could not get localtime.");
		return false;
	}

	// make strftime format
	char	szbuff[1024];
	string	timeform("");
	for(k2hftsvrtplist_t::const_iterator iter = list.begin(); iter != list.end(); ++iter){
		if(iter->is_type_ns){
			sprintf(szbuff, K2HFTSVR_TIME_NS_PRNFORMAT, ts->tv_nsec);
			timeform += szbuff;
		}else{
			timeform += iter->form;
		}
	}

	// make time string
	szbuff[0] = '\0';
	strftime(szbuff, sizeof(szbuff), timeform.c_str(), tm);		// if return 0, we could not diside error or no-error.
	strtime = szbuff;

	return true;
}

inline bool convert_hex_to_char(const char*& ptr, char& data)
{
	data = 0;
	if(!ptr){
		return false;
	}else if('0' <= *ptr && *ptr <= '9'){
		data += (*ptr - '0');
	}else if('a' <= *ptr && *ptr <= 'f'){
		data += ((*ptr - 'a') + 10);
	}else if('A' <= *ptr && *ptr <= 'F'){
		data += ((*ptr - 'A') + 10);
	}else{
		return false;
	}
	const char*	pnext = ptr + sizeof(char);
	if('0' <= *pnext && *pnext <= '9'){
		data *= 16;
		data += (*pnext - '0');
		++ptr;
	}else if('a' <= *pnext && *pnext <= 'f'){
		data *= 16;
		data += ((*pnext - 'a') + 10);
		++ptr;
	}else if('A' <= *pnext && *pnext <= 'F'){
		data *= 16;
		data += ((*pnext - 'A') + 10);
		++ptr;
	}
	return true;
}

inline void build_form_part_list(const char* form, k2hftsvrfplist_t& list)
{
	list.clear();

	if(K2HFT_ISEMPTYSTR(form)){
		MSG_K2HFTPRN("form is empty.");
	}else{
		string				strform = get_string_content(form);
		K2HFTSVRFORMPART	tmppart;
		for(const char* cur = strform.c_str(); '\0' != *cur; ++cur){
			if('%' == *cur){
				++cur;
				if('\0' == *cur){
					MSG_K2HFTPRN("format string(%s) last word is single %%, so skip this word.", form);
					if(!tmppart.form.empty()){
						tmppart.type = K2HFTSVR_FPT_STRING;
						list.push_back(tmppart);
						tmppart.form = "";
					}
					break;

				}else if('%' == *cur){
					// %% -> %
					tmppart.form += *cur;

				}else if('L' == *cur){
					if(!tmppart.form.empty()){
						tmppart.type = K2HFTSVR_FPT_STRING;
						list.push_back(tmppart);
						tmppart.form = "";
					}
					tmppart.type = K2HFTSVR_FPT_CONTENTS;
					list.push_back(tmppart);

				}else if('l' == *cur){
					if(!tmppart.form.empty()){
						tmppart.type = K2HFTSVR_FPT_STRING;
						list.push_back(tmppart);
						tmppart.form = "";
					}
					tmppart.type = K2HFTSVR_FPT_TRIM_CONTENTS;
					list.push_back(tmppart);

				}else if('T' == *cur || 't' == *cur){
					if(!tmppart.form.empty()){
						tmppart.type = K2HFTSVR_FPT_STRING;
						list.push_back(tmppart);
						tmppart.form = "";
					}
					tmppart.type = K2HFTSVR_FPT_TIME;
					list.push_back(tmppart);

				}else if('f' == *cur){
					if(!tmppart.form.empty()){
						tmppart.type = K2HFTSVR_FPT_STRING;
						list.push_back(tmppart);
						tmppart.form = "";
					}
					tmppart.type = K2HFTSVR_FPT_FILENAME;
					list.push_back(tmppart);

				}else if('F' == *cur){
					if(!tmppart.form.empty()){
						tmppart.type = K2HFTSVR_FPT_STRING;
						list.push_back(tmppart);
						tmppart.form = "";
					}
					tmppart.type = K2HFTSVR_FPT_FILEPATH;
					list.push_back(tmppart);

				}else if('P' == *cur || 'p' == *cur){
					if(!tmppart.form.empty()){
						tmppart.type = K2HFTSVR_FPT_STRING;
						list.push_back(tmppart);
						tmppart.form = "";
					}
					tmppart.type = K2HFTSVR_FPT_PID;
					list.push_back(tmppart);

				}else if('H' == *cur || 'h' == *cur){
					if(!tmppart.form.empty()){
						tmppart.type = K2HFTSVR_FPT_STRING;
						list.push_back(tmppart);
						tmppart.form = "";
					}
					tmppart.type = K2HFTSVR_FPT_HOSTNAME;
					list.push_back(tmppart);

				}else{
					MSG_K2HFTPRN("format string(%s) has unknown %%%c format, so skip %% word.", form, *cur);
					tmppart.form += *cur;
				}
			}else if('\\' == *cur){
				++cur;
				if('\0' == *cur || '0' == *cur){		// "\0" means Octal number = 0(0x00)
					MSG_K2HFTPRN("format string(%s) last word is single \\, so skip this word.", form);
					if(!tmppart.form.empty()){
						tmppart.type = K2HFTSVR_FPT_STRING;
						list.push_back(tmppart);
						tmppart.form = "";
					}
					break;
				}else if('\\' == *cur || '\"' == *cur || '\'' == *cur || '\?' == *cur){
					tmppart.form += *cur;
				}else if('a' == *cur){
					tmppart.form += '\a';
				}else if('b' == *cur){
					tmppart.form += '\b';
				}else if('f' == *cur){
					tmppart.form += '\f';
				}else if('n' == *cur){
					tmppart.form += '\n';
				}else if('r' == *cur){
					tmppart.form += '\r';
				}else if('t' == *cur){
					tmppart.form += '\t';
				}else if('x' == *cur){
					++cur;
					char	tmpch = 0;
					if(!convert_hex_to_char(cur, tmpch)){
						if('\0' == *cur){
							MSG_K2HFTPRN("format string(%s) last word is single \\, so skip this word.", form);
							if(!tmppart.form.empty()){
								tmppart.type = K2HFTSVR_FPT_STRING;
								list.push_back(tmppart);
								tmppart.form = "";
							}
							break;
						}else{
							WAN_K2HFTPRN("\"\\xN\" in format string(%s) is wrong format, so skip this string.", form);
						}
					}else{
						tmppart.form += tmpch;
					}
				}else if('0' < *cur && *cur <= '8'){	// "\0" is already checked
					tmppart.form += (*cur - '0');
				}else{
					MSG_K2HFTPRN("format string(%s) last word is single \\, so skip this word.", form);
				}
			}else{
				tmppart.form += *cur;
			}
		}
		if(!tmppart.form.empty()){
			tmppart.type = K2HFTSVR_FPT_STRING;
			list.push_back(tmppart);
		}
	}
}

inline void make_form_output_string(string& stroutput, const k2hftsvrfplist_t& outputform, const k2hftsvrtplist_t& timeform, const char* hostname, pid_t pid, const char* filepath, const struct timespec* ts, const char* content)
{
	string	strtime;
	if(!make_time_output_string(strtime, timeform, ts)){
		WAN_K2HFTPRN("Could not make time formatted string, so skip time formatted string.");
	}

	stroutput = "";
	for(k2hftsvrfplist_t::const_iterator iter = outputform.begin(); iter != outputform.end(); ++iter){
		if(K2HFTSVR_FPT_CONTENTS == iter->type){
			if(content){
				stroutput += content;
			}
		}else if(K2HFTSVR_FPT_TRIM_CONTENTS == iter->type){
			if(content){
				string	strtmp	= trim(string(content));
				stroutput += strtmp;
			}
		}else if(K2HFTSVR_FPT_TIME == iter->type){
			stroutput += strtime;
		}else if(K2HFTSVR_FPT_FILENAME == iter->type){
			string	strtmp;
			if(!get_file_name(filepath, strtmp)){
				WAN_K2HFTPRN("Could not get file name, so skip time formatted file name.");
			}else{
				stroutput += strtmp;
			}
		}else if(K2HFTSVR_FPT_FILEPATH == iter->type){
			if(filepath){
				stroutput += filepath;
			}
		}else if(K2HFTSVR_FPT_PID == iter->type){
			char	szBuff[16];
			sprintf(szBuff, "%d", pid);
			stroutput += szBuff;

		}else if(K2HFTSVR_FPT_HOSTNAME == iter->type){
			if(hostname){
				stroutput += hostname;
			}
		}else{	// K2HFTSVR_FPT_STRING == iter->type
			stroutput += iter->form;
		}
	}
}

//---------------------------------------------------------
// K2hFtSvrInfo
//---------------------------------------------------------
K2hFtSvrInfo::K2hFtSvrInfo(const char* path, K2hFtPluginMan* ppluginman) :
		Config(""), OutputType(K2HFTFSVR_FILE_TYPE), IsBinaryMode(false), IsNoConvert(false), OutputBaseDir(""), UnifyFilePath(""),
		TransConfig(""), DtorCtpPath(K2HFT_K2HTPDTOR), IsTransK2hMemType(true), TransK2hFilePath(""), IsTransK2hInit(true), IsTransK2hFullmap(true),
		TransK2hMaskBitCnt(8), TransK2hCMaskBitCnt(4), TransK2hEleCnt(32), TransK2hPageSize(128), TransK2hThreadCnt(1)
{
	if(!K2HFT_ISEMPTYSTR(path) && ppluginman){
		Load(path, *ppluginman);
	}
}

K2hFtSvrInfo::~K2hFtSvrInfo(void)
{
	Clean();
}

void K2hFtSvrInfo::Clean(void)
{
	Config				= "";
	OutputType			= K2HFTFSVR_FILE_TYPE;
	IsBinaryMode		= false;
	IsNoConvert			= false;
	OutputBaseDir		= "";
	UnifyFilePath		= "";
	TransConfig			= "";
	DtorCtpPath			= K2HFT_K2HTPDTOR;
	IsTransK2hMemType	= true;
	TransK2hFilePath	= "";
	IsTransK2hInit		= true;
	IsTransK2hFullmap	= true;
	TransK2hMaskBitCnt	= 8;
	TransK2hCMaskBitCnt	= 4;
	TransK2hEleCnt		= 32;
	TransK2hPageSize	= 128;
	TransK2hThreadCnt	= 1;
	TimeFormat.clear();
	OutputFormat.clear();

	for(k2hftpluginpath_t::iterator iter = pluginpathmap.begin(); iter != pluginpathmap.end(); pluginpathmap.erase(iter++)){
		PK2HFT_PLUGIN	pPlugin = iter->second;
		K2HFT_DELETE(pPlugin);
	}
}

//
// Do not need to lock in this method, because caller locks before calling.
//
bool K2hFtSvrInfo::Load(const char* config, K2hFtPluginMan& pluginman)
{
	if(K2HFT_ISEMPTYSTR(config)){
		// [NOTE]
		// If using environemnts for configuration, the caller must set this parameter.
		//
		ERR_K2HFTPRN("config parameter is wrong.");
		return false;
	}
	if(IsLoad()){
		ERR_K2HFTPRN("Already load configration file, so could not load %s file.", config ? config : "null");
		return false;
	}

	// get configuration type without environment
	CHMCONFTYPE	conftype = check_chmconf_type(config);
	if(CHMCONF_TYPE_UNKNOWN == conftype || CHMCONF_TYPE_NULL == conftype){
		ERR_K2HFTPRN("Parameter configuration file or json string is wrong.");
		return false;
	}

	bool	result;
	if(CHMCONF_TYPE_INI_FILE == conftype){
		result = LoadIni(config, pluginman);
	}else{
		result = LoadYaml(config, pluginman, (CHMCONF_TYPE_JSON_STRING == conftype));
	}

	if(result){
		// set configuration string and flag
		Config = config;
	}
	return result;
}

bool K2hFtSvrInfo::LoadIni(const char* conffile, K2hFtPluginMan& pluginman)
{
	if(K2HFT_ISEMPTYSTR(conffile)){
		ERR_K2HFTPRN("conffile is empty.");
		return false;
	}

	strlst_t	lines;
	strlst_t	files;
	if(!read_ini_file_contents(conffile, lines, files)){
		ERR_K2HFTPRN("Failed to load oncfigration ini file(%s)", conffile);
		return false;
	}

	// Parse & Check
	PK2HFT_PLUGIN	pPlugin		= NULL;
	bool			set_main_sec= false;
	for(strlst_t::const_iterator iter = lines.begin(); iter != lines.end(); ){
		if((*iter) == INI_K2HFTSVR_MAIN_SEC_STR){
			// main section
			if(set_main_sec){
				ERR_K2HFTPRN("main section(%s) is already set.", INI_K2HFTSVR_MAIN_SEC_STR);
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

				if(INI_K2HFTSVR_TYPE_STR == key){
					// OutputType
					value = upper(value);
					if(INI_K2HFTSVR_FILE1_VAL_STR == value || INI_K2HFTSVR_FILE2_VAL_STR == value){
						OutputType = K2HFTFSVR_FILE_TYPE;
					}else if(INI_K2HFTSVR_TRANS1_VAL_STR == value || INI_K2HFTSVR_TRANS2_VAL_STR == value || INI_K2HFTSVR_TRANS3_VAL_STR == value){
						OutputType = K2HFTFSVR_TRANS_TYPE;
					}else if(INI_K2HFTSVR_BOTH1_VAL_STR == value || INI_K2HFTSVR_BOTH2_VAL_STR == value){
						OutputType = K2HFTFSVR_BOTH_TYPE;
					}else{
						ERR_K2HFTPRN("keyworad(%s)'s value(%s) in main section(%s) is unknown value.", key.c_str(), value.c_str(), INI_K2HFTSVR_MAIN_SEC_STR);
						Clean();
						return false;
					}

				}else if(INI_K2HFTSVR_FILE_BASEDIR_STR == key){
					// OutputBaseDir
					OutputBaseDir = "";
					if(!mkdir_by_abs_path(value.c_str(), OutputBaseDir)){
						ERR_K2HFTPRN("keyworad(%s)'s value(%s) in main section(%s), could not make directory by something error.", key.c_str(), value.c_str(), INI_K2HFTSVR_MAIN_SEC_STR);
						Clean();
						return false;
					}
					OutputBaseDir += '/';			// terminated '/'

				}else if(INI_K2HFTSVR_FILE_UNIFY_STR == key){
					// UnifyFilePath
					//
					// This value is relative path from OutputBaseDir, so we check this after.
					//
					UnifyFilePath = value;

				}else if(INI_K2HFTSVR_FILE_TIMEFORM_STR == key){
					// TimeFormat
					build_time_part_list(value.c_str(), TimeFormat);

				}else if(INI_K2HFTSVR_PLUGIN_STR == key){
					// Plugin
					if(pPlugin){
						WAN_K2HFTPRN("keyworad(%s) in rule section(%s) is already set, then it is over wrote by new value(%s),", key.c_str(), INI_K2HFTSVR_MAIN_SEC_STR, value.c_str());
						K2HFT_DELETE(pPlugin);
					}
					pPlugin				= new K2HFT_PLUGIN;
					pPlugin->BaseParam	= value;

				}else if(INI_K2HFTSVR_FORMAT_STR == key){
					// OutputFormat
					build_form_part_list(value.c_str(), OutputFormat);

				}else if(INI_K2HFTSVR_BINTRANS_STR == key){
					// IsBinaryMode
					value = upper(value);
					if(INI_K2HFT_YES1_VAL_STR == value || INI_K2HFT_YES2_VAL_STR == value || INI_K2HFT_ON_VAL_STR == value){
						IsBinaryMode = true;
					}else if(INI_K2HFT_NO1_VAL_STR == value || INI_K2HFT_NO2_VAL_STR == value || INI_K2HFT_OFF_VAL_STR == value){
						IsBinaryMode = false;
					}else{
						WAN_K2HFTPRN("keyworad(%s)'s value(%s) in main section(%s) is unknown value, so skip it.", key.c_str(), value.c_str(), INI_K2HFTSVR_MAIN_SEC_STR);
					}

				}else if(INI_K2HFTSVR_TRANSCONF_STR == key){
					// TransConfig
					TransConfig = "";
					if(!value.empty() && right_check_json_string(value.c_str())){
						// json string
						TransConfig = value;
					}else{
						// not json string, thus it is a file
						if(!check_path_real_path(value.c_str(), TransConfig)){
							ERR_K2HFTPRN("keyworad(%s)'s value(%s) in main section(%s) does not exist.", key.c_str(), value.c_str(), INI_K2HFTSVR_MAIN_SEC_STR);
							Clean();
							return false;
						}
					}

				}else if(INI_K2HFTSVR_K2HTYPE_STR == key){
					// IsTransK2hMemType
					value = upper(value);
					if(INI_K2HFT_MEM1_VAL_STR == value || INI_K2HFT_MEM2_VAL_STR == value || INI_K2HFT_MEM3_VAL_STR == value){
						IsTransK2hMemType	= true;
					}else if(INI_K2HFT_TEMP1_VAL_STR == value || INI_K2HFT_TEMP2_VAL_STR == value || INI_K2HFT_TEMP3_VAL_STR == value){
						IsTransK2hMemType	= false;
					}else if(INI_K2HFT_FILE1_VAL_STR == value || INI_K2HFT_FILE2_VAL_STR == value){
						IsTransK2hMemType	= false;
					}else{
						WAN_K2HFTPRN("keyworad(%s)'s value(%s) in main section(%s) is unknown value, so skip it.", key.c_str(), value.c_str(), INI_K2HFTSVR_MAIN_SEC_STR);
					}

				}else if(INI_K2HFTSVR_K2HFILE_STR == key){
					// TransK2hFilePath
					TransK2hFilePath = "";
					if(!check_path_real_path(value.c_str(), TransK2hFilePath)){
						MSG_K2HFTPRN("keyworad(%s)'s value(%s) in main section(%s) does not exist.", key.c_str(), value.c_str(), INI_K2HFTSVR_MAIN_SEC_STR);
						// not abspath.
						TransK2hFilePath = value;
					}

				}else if(INI_K2HFTSVR_K2HFULLMAP_STR == key){
					// IsTransK2hFullmap
					value = upper(value);
					if(INI_K2HFT_YES1_VAL_STR == value || INI_K2HFT_YES2_VAL_STR == value || INI_K2HFT_ON_VAL_STR == value){
						IsTransK2hFullmap = true;
					}else if(INI_K2HFT_NO1_VAL_STR == value || INI_K2HFT_NO2_VAL_STR == value || INI_K2HFT_OFF_VAL_STR == value){
						IsTransK2hFullmap = false;
					}else{
						WAN_K2HFTPRN("keyworad(%s)'s value(%s) in main section(%s) is unknown value, so skip it.", key.c_str(), value.c_str(), INI_K2HFTSVR_MAIN_SEC_STR);
					}

				}else if(INI_K2HFTSVR_K2HINIT_STR == key){
					// IsTransK2hInit
					value = upper(value);
					if(INI_K2HFT_YES1_VAL_STR == value || INI_K2HFT_YES2_VAL_STR == value || INI_K2HFT_ON_VAL_STR == value){
						IsTransK2hInit = true;
					}else if(INI_K2HFT_NO1_VAL_STR == value || INI_K2HFT_NO2_VAL_STR == value || INI_K2HFT_OFF_VAL_STR == value){
						IsTransK2hInit = false;
					}else{
						WAN_K2HFTPRN("keyworad(%s)'s value(%s) in main section(%s) is unknown value, so skip it.", key.c_str(), value.c_str(), INI_K2HFTSVR_MAIN_SEC_STR);
					}

				}else if(INI_K2HFTSVR_K2HMASKBIT_STR == key){
					// TransK2hMaskBitCnt
					TransK2hMaskBitCnt = atoi(value.c_str());

				}else if(INI_K2HFTSVR_K2HCMASKBIT_STR == key){
					// TransK2hCMaskBitCnt
					TransK2hCMaskBitCnt = atoi(value.c_str());

				}else if(INI_K2HFTSVR_K2HMAXELE_STR == key){
					// TransK2hEleCnt
					TransK2hEleCnt = atoi(value.c_str());

				}else if(INI_K2HFTSVR_K2HPAGESIZE_STR == key){
					// TransK2hPageSize
					TransK2hPageSize = static_cast<size_t>(atoll(value.c_str()));

				}else if(INI_K2HFTSVR_DTORTHREADCNT_STR == key){
					// TransK2hThreadCnt
					int	nTmp = atoi(value.c_str());
					if(0 >= nTmp){
						ERR_K2HFTPRN("keyworad(%s)'s value(%s) in main section(%s) must be over 0.", key.c_str(), value.c_str(), INI_K2HFTSVR_MAIN_SEC_STR);
						Clean();
						return false;
					}
					TransK2hThreadCnt = nTmp;

				}else if(INI_K2HFTSVR_DTORCTP_STR == key){
					// DtorCtpPath
					DtorCtpPath = K2HFT_K2HTPDTOR;
					if(!check_path_real_path(value.c_str(), DtorCtpPath)){
						ERR_K2HFTPRN("keyworad(%s)'s value(%s) in main section(%s) does not exist.", key.c_str(), value.c_str(), INI_K2HFTSVR_MAIN_SEC_STR);
						Clean();
						return false;
					}

				}else{
					WAN_K2HFTPRN("Unknown keyworad(%s) in main section(%s), so skip it and continue...", key.c_str(), INI_K2HFTSVR_MAIN_SEC_STR);
				}
			}
		}else if(INI_SEC_START_CHAR == (*iter)[0] && INI_SEC_END_CHAR == (*iter)[iter->length() - 1]){
			// Unknown section.
			for(++iter; iter != lines.end(); ++iter){
				if(INI_SEC_START_CHAR == (*iter)[0] && INI_SEC_END_CHAR == (*iter)[iter->length() - 1]){
					// another section start, so break this loop
					break;
				}
			}
		}else{
			++iter;
		}
	}

	// check values
	IsNoConvert = false;
	if(IS_K2HFTFSVR_FILE_TYPE(OutputType)){
		// double check
		if(!IS_K2HFTFSVR_TRANS_TYPE(OutputType)){
			if(!TransConfig.empty()){
				WAN_K2HFTPRN("Specified Output file type, but keyworad(%s) was specified.", INI_K2HFTSVR_TRANSCONF_STR);
				TransConfig = "";
			}
			if(!TransK2hFilePath.empty()){
				WAN_K2HFTPRN("Specified Output file type, but keyworad(%s) was specified.", INI_K2HFTSVR_K2HFILE_STR);
				TransK2hFilePath = "";
			}
		}
		if(OutputBaseDir.empty()){
			ERR_K2HFTPRN("Specified Output file type, but keyworad(%s) was not specified. It must be set.", INI_K2HFTSVR_FILE_BASEDIR_STR);
			Clean();
			return false;
		}
		if(!UnifyFilePath.empty()){
			// check & convert relative to absolute path.
			string	tmp		= OutputBaseDir + UnifyFilePath;
			UnifyFilePath	= "";
			if(!make_file_by_abs_path(tmp.c_str(), (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH), UnifyFilePath, true)){
				ERR_K2HFTPRN("Specified Output file type, and keyworad(%s) set output unify file path(%s). but could not make file.", INI_K2HFTSVR_FILE_UNIFY_STR, tmp.c_str());
				Clean();
				return false;
			}
		}
		if(0 == OutputFormat.size()){
			// set default
			K2HFTSVRFORMPART	tmppart;
			tmppart.type = K2HFTSVR_FPT_CONTENTS;
			OutputFormat.push_back(tmppart);
			IsNoConvert = true;
		}else{
			if(1 == OutputFormat.size()){
				K2HFTSVRFORMPART	tmppart = OutputFormat.front();
				if(K2HFTSVR_FPT_CONTENTS == tmppart.type){
					IsNoConvert = true;
				}
			}
			// check time format
			for(k2hftsvrfplist_t::const_iterator iter = OutputFormat.begin(); iter != OutputFormat.end(); ++iter){
				if(K2HFTSVR_FPT_TIME == iter->type){
					if(0 == TimeFormat.size()){
						WAN_K2HFTPRN("Output file format has '%%T', but %s is not specified.", INI_K2HFTSVR_FILE_TIMEFORM_STR);
					}
					break;
				}
			}
		}

		// plugin
		if(pPlugin){
			pPlugin->OutputPath	= IsUnityFileMode() ? UnifyFilePath : OutputBaseDir;
			pPlugin->mode		= S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH | S_IFREG;
			pPlugin->not_execute= !IsUnityFileMode();

			// add plugin to manager
			if(!pluginman.AddPluginInfo(pPlugin)){
				ERR_K2HFTPRN("keyworad(%s) in rule section(%s) is specified, but failed to adding it to plugin manager.", INI_K2HFTSVR_PLUGIN_STR, INI_K2HFTSVR_MAIN_SEC_STR);
				Clean();
				return false;
			}
			// add plugin to mapping
			pluginpathmap[pPlugin->OutputPath] = pPlugin;
		}
	}

	if(IS_K2HFTFSVR_TRANS_TYPE(OutputType)){
		// double check
		if(!IS_K2HFTFSVR_FILE_TYPE(OutputType)){
			if(pPlugin){
				WAN_K2HFTPRN("Specified keyworad(%s) in rule section(%s), but transfer mode is enabled. so plugin is ignored.", INI_K2HFTSVR_PLUGIN_STR, INI_K2HFTSVR_MAIN_SEC_STR);
				K2HFT_DELETE(pPlugin);
			}
			// transfer type
			if(!OutputBaseDir.empty()){
				WAN_K2HFTPRN("Specified Output file type, but keyworad(%s) was specified.", INI_K2HFTSVR_FILE_BASEDIR_STR);
				OutputBaseDir = "";
			}
			if(!UnifyFilePath.empty()){
				WAN_K2HFTPRN("Specified Output file type, but keyworad(%s) was specified.", INI_K2HFTSVR_FILE_UNIFY_STR);
				UnifyFilePath = "";
			}
			if(0 != TimeFormat.size()){
				WAN_K2HFTPRN("Specified Output file type, but keyworad(%s) was specified.", INI_K2HFTSVR_FILE_TIMEFORM_STR);
				TimeFormat.clear();
			}
			if(0 != OutputFormat.size()){
				WAN_K2HFTPRN("Specified Output file type, but keyworad(%s) was specified.", INI_K2HFTSVR_FORMAT_STR);
				OutputFormat.clear();
			}
		}
		if(TransConfig.empty()){
			MSG_K2HFTPRN("Specified Transfer type, but keyworad(%s) was not specified. You must set environemnts for transfer configuration", INI_K2HFTSVR_TRANSCONF_STR);
		}
		if(IsTransK2hMemType && !TransK2hFilePath.empty()){
			WAN_K2HFTPRN("Specified Transfer type and k2hash is memory tyoe, but keyworad(%s) was specified. So it is ignored.", INI_K2HFTSVR_K2HFILE_STR);
			TransK2hFilePath = "";
		}else if(!IsTransK2hMemType && TransK2hFilePath.empty()){
			ERR_K2HFTPRN("Specified Transfer type and k2hash is file tyoe, but keyworad(%s) was not specified. It must be set.", INI_K2HFTSVR_K2HFILE_STR);
			Clean();
			return false;
		}
		if(IsTransK2hMemType && !IsTransK2hInit){
			WAN_K2HFTPRN("Specified Transfer type and k2hash is memory tyoe, but keyworad(%s) was \"no\". So it is ignored(must be initializing).", INI_K2HFTSVR_K2HINIT_STR);
			IsTransK2hInit = true;
		}
	}
	return true;
}


bool K2hFtSvrInfo::LoadYaml(const char* config, K2hFtPluginMan& pluginman, bool is_json_string)
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
	bool	result	= LoadYamlTopLevel(yparser, pluginman);

	yaml_parser_delete(&yparser);
	if(fp){
		fclose(fp);
	}
	return result;
}

bool K2hFtSvrInfo::LoadYamlTopLevel(yaml_parser_t& yparser, K2hFtPluginMan& pluginman)
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
					// Found Top Level Keywards, start to loading
					if(0 == strcasecmp(K2HFTSVR_MAIN_SEC_STR, reinterpret_cast<const char*>(yevent.data.scalar.value))){
						if(is_set_main){
							MSG_K2HFTPRN("Got yaml scalar event in loop, but already loading %s. Thus stacks this event.", K2HFTSVR_MAIN_SEC_STR);
							if(!other_stack.add(yevent.type)){
								result = false;
							}
						}else{
							// Load K2HFTSVR_MAIN_SEC_STR section
							if(!LoadYamlMainSec(yparser, pluginman)){
								ERR_K2HFTPRN("Something error occured in loading %s section.", K2HFTSVR_MAIN_SEC_STR);
								result = false;
							}
						}
					}else{
						MSG_K2HFTPRN("Got yaml scalar event in loop, but unknown keyward(%s) for me. Thus stacks this event.", reinterpret_cast<const char*>(yevent.data.scalar.value));
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

bool K2hFtSvrInfo::LoadYamlMainSec(yaml_parser_t& yparser, K2hFtPluginMan& pluginman)
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
	string			key("");
	PK2HFT_PLUGIN	pPlugin= NULL;
	bool			result = true;
	for(bool is_loop = true; is_loop && result; ){
		// get event
		if(!yaml_parser_parse(&yparser, &yevent)){
			ERR_K2HFTPRN("Could not parse event. errno = %d", errno);
			result = false;
			continue;
		}

		// check event
		if(YAML_MAPPING_END_EVENT == yevent.type){
			// End of mapping event
			is_loop = false;

		}else if(YAML_SCALAR_EVENT == yevent.type){
			// Load key & value
			if(key.empty()){
				key = reinterpret_cast<const char*>(yevent.data.scalar.value);
			}else{
				//
				// Compare key and set value
				//
				if(0 == strcasecmp(INI_K2HFTSVR_TYPE_STR, key.c_str())){
					// OutputType
					string	value = upper(reinterpret_cast<const char*>(yevent.data.scalar.value));
					if(INI_K2HFTSVR_FILE1_VAL_STR == value || INI_K2HFTSVR_FILE2_VAL_STR == value){
						OutputType = K2HFTFSVR_FILE_TYPE;
					}else if(INI_K2HFTSVR_TRANS1_VAL_STR == value || INI_K2HFTSVR_TRANS2_VAL_STR == value || INI_K2HFTSVR_TRANS3_VAL_STR == value){
						OutputType = K2HFTFSVR_TRANS_TYPE;
					}else if(INI_K2HFTSVR_BOTH1_VAL_STR == value || INI_K2HFTSVR_BOTH2_VAL_STR == value){
						OutputType = K2HFTFSVR_BOTH_TYPE;
					}else{
						ERR_K2HFTPRN("keyworad(%s)'s value(%s) in main section(%s) is unknown value.", key.c_str(), reinterpret_cast<const char*>(yevent.data.scalar.value), K2HFTSVR_MAIN_SEC_STR);
						result = false;
					}

				}else if(0 == strcasecmp(INI_K2HFTSVR_FILE_BASEDIR_STR, key.c_str())){
					// OutputBaseDir
					OutputBaseDir = "";
					if(!mkdir_by_abs_path(reinterpret_cast<const char*>(yevent.data.scalar.value), OutputBaseDir)){
						ERR_K2HFTPRN("keyworad(%s)'s value(%s) in main section(%s), could not make directory by something error.", key.c_str(), reinterpret_cast<const char*>(yevent.data.scalar.value), K2HFTSVR_MAIN_SEC_STR);
						result = false;
					}else{
						OutputBaseDir += '/';			// terminated '/'
					}

				}else if(0 == strcasecmp(INI_K2HFTSVR_FILE_UNIFY_STR, key.c_str())){
					// UnifyFilePath
					//
					// This value is relative path from OutputBaseDir, so we check this after.
					//
					UnifyFilePath = reinterpret_cast<const char*>(yevent.data.scalar.value);

				}else if(0 == strcasecmp(INI_K2HFTSVR_FILE_TIMEFORM_STR, key.c_str())){
					// TimeFormat
					build_time_part_list(reinterpret_cast<const char*>(yevent.data.scalar.value), TimeFormat);

				}else if(0 == strcasecmp(INI_K2HFTSVR_PLUGIN_STR, key.c_str())){
					// Plugin
					if(pPlugin){
						WAN_K2HFTPRN("keyworad(%s) in rule section(%s) is already set, then it is over wrote by new value(%s),", key.c_str(), K2HFTSVR_MAIN_SEC_STR, reinterpret_cast<const char*>(yevent.data.scalar.value));
						K2HFT_DELETE(pPlugin);
					}
					pPlugin				= new K2HFT_PLUGIN;
					pPlugin->BaseParam	= reinterpret_cast<const char*>(yevent.data.scalar.value);

				}else if(0 == strcasecmp(INI_K2HFTSVR_FORMAT_STR, key.c_str())){
					// OutputFormat
					build_form_part_list(reinterpret_cast<const char*>(yevent.data.scalar.value), OutputFormat);

				}else if(0 == strcasecmp(INI_K2HFTSVR_BINTRANS_STR, key.c_str())){
					// IsBinaryMode
					string	value = upper(reinterpret_cast<const char*>(yevent.data.scalar.value));
					if(INI_K2HFT_YES1_VAL_STR == value || INI_K2HFT_YES2_VAL_STR == value || INI_K2HFT_ON_VAL_STR == value){
						IsBinaryMode = true;
					}else if(INI_K2HFT_NO1_VAL_STR == value || INI_K2HFT_NO2_VAL_STR == value || INI_K2HFT_OFF_VAL_STR == value){
						IsBinaryMode = false;
					}else{
						WAN_K2HFTPRN("keyworad(%s)'s value(%s) in main section(%s) is unknown value, so skip it.", key.c_str(), reinterpret_cast<const char*>(yevent.data.scalar.value), K2HFTSVR_MAIN_SEC_STR);
					}

				}else if(0 == strcasecmp(INI_K2HFTSVR_TRANSCONF_STR, key.c_str())){
					// TransConfig
					TransConfig = "";
					if(!K2HFT_ISEMPTYSTR(reinterpret_cast<const char*>(yevent.data.scalar.value)) && right_check_json_string(reinterpret_cast<const char*>(yevent.data.scalar.value))){
						// json string
						TransConfig = reinterpret_cast<const char*>(yevent.data.scalar.value);
					}else{
						// not json string, thus it is a file
						if(!check_path_real_path(reinterpret_cast<const char*>(yevent.data.scalar.value), TransConfig)){
							ERR_K2HFTPRN("keyworad(%s)'s value(%s) in main section(%s) does not exist.", key.c_str(), reinterpret_cast<const char*>(yevent.data.scalar.value), INI_K2HFTSVR_MAIN_SEC_STR);
							result = false;
						}
					}

				}else if(0 == strcasecmp(INI_K2HFTSVR_K2HTYPE_STR, key.c_str())){
					// IsTransK2hMemType
					string	value = upper(reinterpret_cast<const char*>(yevent.data.scalar.value));
					if(INI_K2HFT_MEM1_VAL_STR == value || INI_K2HFT_MEM2_VAL_STR == value || INI_K2HFT_MEM3_VAL_STR == value){
						IsTransK2hMemType	= true;
					}else if(INI_K2HFT_TEMP1_VAL_STR == value || INI_K2HFT_TEMP2_VAL_STR == value || INI_K2HFT_TEMP3_VAL_STR == value){
						IsTransK2hMemType	= false;
					}else if(INI_K2HFT_FILE1_VAL_STR == value || INI_K2HFT_FILE2_VAL_STR == value){
						IsTransK2hMemType	= false;
					}else{
						WAN_K2HFTPRN("keyworad(%s)'s value(%s) in main section(%s) is unknown value, so skip it.", key.c_str(), reinterpret_cast<const char*>(yevent.data.scalar.value), K2HFTSVR_MAIN_SEC_STR);
					}

				}else if(0 == strcasecmp(INI_K2HFTSVR_K2HFILE_STR, key.c_str())){
					// TransK2hFilePath
					TransK2hFilePath = "";
					if(!check_path_real_path(reinterpret_cast<const char*>(yevent.data.scalar.value), TransK2hFilePath)){
						MSG_K2HFTPRN("keyworad(%s)'s value(%s) in main section(%s) does not exist.", key.c_str(), reinterpret_cast<const char*>(yevent.data.scalar.value), INI_K2HFTSVR_MAIN_SEC_STR);
						// not abspath.
						TransK2hFilePath = reinterpret_cast<const char*>(yevent.data.scalar.value);
					}

				}else if(0 == strcasecmp(INI_K2HFTSVR_K2HFULLMAP_STR, key.c_str())){
					// IsTransK2hFullmap
					string	value = upper(reinterpret_cast<const char*>(yevent.data.scalar.value));
					if(INI_K2HFT_YES1_VAL_STR == value || INI_K2HFT_YES2_VAL_STR == value || INI_K2HFT_ON_VAL_STR == value){
						IsTransK2hFullmap = true;
					}else if(INI_K2HFT_NO1_VAL_STR == value || INI_K2HFT_NO2_VAL_STR == value || INI_K2HFT_OFF_VAL_STR == value){
						IsTransK2hFullmap = false;
					}else{
						WAN_K2HFTPRN("keyworad(%s)'s value(%s) in main section(%s) is unknown value, so skip it.", key.c_str(), reinterpret_cast<const char*>(yevent.data.scalar.value), K2HFTSVR_MAIN_SEC_STR);
					}

				}else if(0 == strcasecmp(INI_K2HFTSVR_K2HINIT_STR, key.c_str())){
					// IsTransK2hInit
					string	value = upper(reinterpret_cast<const char*>(yevent.data.scalar.value));
					if(INI_K2HFT_YES1_VAL_STR == value || INI_K2HFT_YES2_VAL_STR == value || INI_K2HFT_ON_VAL_STR == value){
						IsTransK2hInit = true;
					}else if(INI_K2HFT_NO1_VAL_STR == value || INI_K2HFT_NO2_VAL_STR == value || INI_K2HFT_OFF_VAL_STR == value){
						IsTransK2hInit = false;
					}else{
						WAN_K2HFTPRN("keyworad(%s)'s value(%s) in main section(%s) is unknown value, so skip it.", key.c_str(), reinterpret_cast<const char*>(yevent.data.scalar.value), K2HFTSVR_MAIN_SEC_STR);
					}

				}else if(0 == strcasecmp(INI_K2HFTSVR_K2HMASKBIT_STR, key.c_str())){
					// TransK2hMaskBitCnt
					TransK2hMaskBitCnt = atoi(reinterpret_cast<const char*>(yevent.data.scalar.value));

				}else if(0 == strcasecmp(INI_K2HFTSVR_K2HCMASKBIT_STR, key.c_str())){
					// TransK2hCMaskBitCnt
					TransK2hCMaskBitCnt = atoi(reinterpret_cast<const char*>(yevent.data.scalar.value));

				}else if(0 == strcasecmp(INI_K2HFTSVR_K2HMAXELE_STR, key.c_str())){
					// TransK2hEleCnt
					TransK2hEleCnt = atoi(reinterpret_cast<const char*>(yevent.data.scalar.value));

				}else if(0 == strcasecmp(INI_K2HFTSVR_K2HPAGESIZE_STR, key.c_str())){
					// TransK2hPageSize
					TransK2hPageSize = static_cast<size_t>(atoll(reinterpret_cast<const char*>(yevent.data.scalar.value)));

				}else if(0 == strcasecmp(INI_K2HFTSVR_DTORTHREADCNT_STR, key.c_str())){
					// TransK2hThreadCnt
					int	nTmp = atoi(reinterpret_cast<const char*>(yevent.data.scalar.value));
					if(0 >= nTmp){
						ERR_K2HFTPRN("keyworad(%s)'s value(%s) in main section(%s) must be over 0.", key.c_str(), reinterpret_cast<const char*>(yevent.data.scalar.value), K2HFTSVR_MAIN_SEC_STR);
						result = false;
					}else{
						TransK2hThreadCnt = nTmp;
					}

				}else if(0 == strcasecmp(INI_K2HFTSVR_DTORCTP_STR, key.c_str())){
					// DtorCtpPath
					DtorCtpPath = K2HFT_K2HTPDTOR;
					if(!check_path_real_path(reinterpret_cast<const char*>(yevent.data.scalar.value), DtorCtpPath)){
						ERR_K2HFTPRN("keyworad(%s)'s value(%s) in main section(%s) does not exist.", key.c_str(), reinterpret_cast<const char*>(yevent.data.scalar.value), K2HFTSVR_MAIN_SEC_STR);
						result = false;
					}
				}else{
					WAN_K2HFTPRN("Found unexpected key(%s) in %s section, thus skip this key and value.", key.c_str(), K2HFTSVR_MAIN_SEC_STR);
				}
				key.clear();
			}
		}else{
			// [TODO] Now not support alias(anchor) event
			//
			ERR_K2HFTPRN("Found unexpected yaml event(%d) in %s section.", yevent.type, K2HFTSVR_MAIN_SEC_STR);
			result = false;
		}

		// delete event
		if(is_loop){
			is_loop = yevent.type != YAML_STREAM_END_EVENT;
		}
		yaml_event_delete(&yevent);
	}

	// check values
	if(result){
		IsNoConvert = false;
		if(IS_K2HFTFSVR_FILE_TYPE(OutputType)){
			// double check
			if(!IS_K2HFTFSVR_TRANS_TYPE(OutputType)){
				if(!TransConfig.empty()){
					WAN_K2HFTPRN("Specified Output file type, but keyworad(%s) was specified.", INI_K2HFTSVR_TRANSCONF_STR);
					TransConfig = "";
				}
				if(!TransK2hFilePath.empty()){
					WAN_K2HFTPRN("Specified Output file type, but keyworad(%s) was specified.", INI_K2HFTSVR_K2HFILE_STR);
					TransK2hFilePath = "";
				}
			}
			if(OutputBaseDir.empty()){
				ERR_K2HFTPRN("Specified Output file type, but keyworad(%s) was not specified. It must be set.", INI_K2HFTSVR_FILE_BASEDIR_STR);
				Clean();
				return false;
			}
			if(!UnifyFilePath.empty()){
				// check & convert relative to absolute path.
				string	tmp		= OutputBaseDir + UnifyFilePath;
				UnifyFilePath	= "";
				if(!make_file_by_abs_path(tmp.c_str(), (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH), UnifyFilePath, true)){
					ERR_K2HFTPRN("Specified Output file type, and keyworad(%s) set output unify file path(%s). but could not make file.", INI_K2HFTSVR_FILE_UNIFY_STR, tmp.c_str());
					Clean();
					return false;
				}
			}
			if(0 == OutputFormat.size()){
				// set default
				K2HFTSVRFORMPART	tmppart;
				tmppart.type = K2HFTSVR_FPT_CONTENTS;
				OutputFormat.push_back(tmppart);
				IsNoConvert = true;
			}else{
				if(1 == OutputFormat.size()){
					K2HFTSVRFORMPART	tmppart = OutputFormat.front();
					if(K2HFTSVR_FPT_CONTENTS == tmppart.type){
						IsNoConvert = true;
					}
				}
				// check time format
				for(k2hftsvrfplist_t::const_iterator iter = OutputFormat.begin(); iter != OutputFormat.end(); ++iter){
					if(K2HFTSVR_FPT_TIME == iter->type){
						if(0 == TimeFormat.size()){
							WAN_K2HFTPRN("Output file format has '%%T', but %s is not specified.", INI_K2HFTSVR_FILE_TIMEFORM_STR);
						}
						break;
					}
				}
			}

			// plugin
			if(pPlugin){
				pPlugin->OutputPath	= IsUnityFileMode() ? UnifyFilePath : OutputBaseDir;
				pPlugin->mode		= S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH | S_IFREG;
				pPlugin->not_execute= !IsUnityFileMode();

				// add plugin to manager
				if(!pluginman.AddPluginInfo(pPlugin)){
					ERR_K2HFTPRN("keyworad(%s) in rule section(%s) is specified, but failed to adding it to plugin manager.", INI_K2HFTSVR_PLUGIN_STR, K2HFTSVR_MAIN_SEC_STR);
					Clean();
					return false;
				}
				// add plugin to mapping
				pluginpathmap[pPlugin->OutputPath] = pPlugin;
			}
		}

		if(IS_K2HFTFSVR_TRANS_TYPE(OutputType)){
			// double check
			if(!IS_K2HFTFSVR_FILE_TYPE(OutputType)){
				if(pPlugin){
					WAN_K2HFTPRN("Specified keyworad(%s) in rule section(%s), but transfer mode is enabled. so plugin is ignored.", INI_K2HFTSVR_PLUGIN_STR, K2HFTSVR_MAIN_SEC_STR);
					K2HFT_DELETE(pPlugin);
				}
				// transfer type
				if(!OutputBaseDir.empty()){
					WAN_K2HFTPRN("Specified Output file type, but keyworad(%s) was specified.", INI_K2HFTSVR_FILE_BASEDIR_STR);
					OutputBaseDir = "";
				}
				if(!UnifyFilePath.empty()){
					WAN_K2HFTPRN("Specified Output file type, but keyworad(%s) was specified.", INI_K2HFTSVR_FILE_UNIFY_STR);
					UnifyFilePath = "";
				}
				if(0 != TimeFormat.size()){
					WAN_K2HFTPRN("Specified Output file type, but keyworad(%s) was specified.", INI_K2HFTSVR_FILE_TIMEFORM_STR);
					TimeFormat.clear();
				}
				if(0 != OutputFormat.size()){
					WAN_K2HFTPRN("Specified Output file type, but keyworad(%s) was specified.", INI_K2HFTSVR_FORMAT_STR);
					OutputFormat.clear();
				}
			}
			if(TransConfig.empty()){
				MSG_K2HFTPRN("Specified Transfer type, but keyworad(%s) was not specified. You must set environemnts for transfer configuration", INI_K2HFTSVR_TRANSCONF_STR);
			}
			if(IsTransK2hMemType && !TransK2hFilePath.empty()){
				WAN_K2HFTPRN("Specified Transfer type and k2hash is memory tyoe, but keyworad(%s) was specified. So it is ignored.", INI_K2HFTSVR_K2HFILE_STR);
				TransK2hFilePath = "";
			}else if(!IsTransK2hMemType && TransK2hFilePath.empty()){
				ERR_K2HFTPRN("Specified Transfer type and k2hash is file tyoe, but keyworad(%s) was not specified. It must be set.", INI_K2HFTSVR_K2HFILE_STR);
				Clean();
				return false;
			}
			if(IsTransK2hMemType && !IsTransK2hInit){
				WAN_K2HFTPRN("Specified Transfer type and k2hash is memory tyoe, but keyworad(%s) was \"no\". So it is ignored(must be initializing).", INI_K2HFTSVR_K2HINIT_STR);
				IsTransK2hInit = true;
			}
		}
	}else{
		Clean();
	}
	return result;
}

bool K2hFtSvrInfo::Dump(void) const
{
	if(!IsLoad()){
		MSG_K2HFTPRN("Not load Configration yet.");
		return false;
	}

	string	timefrm("");
	for(k2hftsvrtplist_t::const_iterator iter = TimeFormat.begin(); iter != TimeFormat.end(); ++iter){
		if(iter->is_type_ns){
			timefrm += K2HFTSVR_TIME_NS_KEYWORD;
		}else{
			timefrm += iter->form;
		}
	}
	string	outputfrm("");
	for(k2hftsvrfplist_t::const_iterator iter = OutputFormat.begin(); iter != OutputFormat.end(); ++iter){
		if(K2HFTSVR_FPT_CONTENTS == iter->type){
			outputfrm += "%L";
		}else if(K2HFTSVR_FPT_TIME == iter->type){
			outputfrm += "%T";
		}else if(K2HFTSVR_FPT_FILENAME == iter->type){
			outputfrm += "%f";
		}else if(K2HFTSVR_FPT_FILEPATH == iter->type){
			outputfrm += "%F";
		}else if(K2HFTSVR_FPT_PID == iter->type){
			outputfrm += "%P";
		}else if(K2HFTSVR_FPT_HOSTNAME == iter->type){
			outputfrm += "%H";
		}else{	// K2HFTSVR_FPT_STRING == iter->type
			outputfrm += iter->form;
		}
	}

	// common
	K2HFTPRN("Configration(file or json)   : %s",	Config.c_str());

	K2HFTPRN("k2hftfuisesvr = {");
	K2HFTPRN("  type                       : %s",	STR_K2HFTFSVR_TYPE(OutputType));
	K2HFTPRN("  output base directory      : %s",	OutputBaseDir.c_str());
	K2HFTPRN("  unity file path            : %s",	UnifyFilePath.c_str());

	K2HFTPRN("  transfer configuration     : %s",	TransConfig.c_str());
	K2HFTPRN("  trans k2hdtor plugin       : %s",	DtorCtpPath.c_str());
	K2HFTPRN("  trans k2hash type          : %s",	IsTransK2hMemType ? "memory" : "file");
	K2HFTPRN("  trans k2hash file path     : %s",	TransK2hFilePath.c_str());
	K2HFTPRN("  trans k2hash initialize    : %s",	IsTransK2hInit ? "yes" : "no");
	K2HFTPRN("  trans k2hash full mapping  : %s",	IsTransK2hFullmap ? "yes" : "no");
	K2HFTPRN("  trans k2hash mask bit      : %d",	TransK2hMaskBitCnt);
	K2HFTPRN("  trans k2hash cmask bit     : %d",	TransK2hCMaskBitCnt);
	K2HFTPRN("  trans k2hash element cnt   : %d",	TransK2hEleCnt);
	K2HFTPRN("  trans k2hash page size     : %zu",	TransK2hPageSize);
	K2HFTPRN("  trans k2hash dtor thread   : %d",	TransK2hThreadCnt);

	K2HFTPRN("  time data format           : \"%s\"",	timefrm.c_str());
	K2HFTPRN("  data format                : \"%s\"",	outputfrm.c_str());
	K2HFTPRN("}");

	return true;
}

// [NOTE]
// If IsNoConvert is true, chould not call this method.
// You can get output data which is as same as content, use content instead of converted stroutput.
//
bool K2hFtSvrInfo::ConvertOutput(string& stroutput, const char* hostname, pid_t pid, const char* filepath, const struct timespec* ts, const char* content) const
{
	if(!IS_K2HFTFSVR_FILE_TYPE(OutputType)){
		ERR_K2HFTPRN("This is transfer mode, so could not convert string by formatting.");
		return false;
	}
	if(IsBinaryMode){
		ERR_K2HFTPRN("This is binary mode, so could not convert string by formatting.");
		return false;
	}
	// make output
	make_form_output_string(stroutput, OutputFormat, TimeFormat, hostname, pid, filepath, ts, content);

	return true;
}

PK2HFT_PLUGIN K2hFtSvrInfo::FindPlugin(string& outputpath)
{
	k2hftpluginpath_t::iterator iter;
	if(pluginpathmap.end() == (iter = pluginpathmap.find(outputpath))){
		return NULL;
	}
	return iter->second;
}

//
// basedir is terminated by '/'
//
PK2HFT_PLUGIN K2hFtSvrInfo::GetPlugin(string& outputpath, string& basedir, K2hFtPluginMan& pluginman)
{
	PK2HFT_PLUGIN	pPlugin = FindPlugin(outputpath);
	if(pPlugin){
		return pPlugin;
	}
	PK2HFT_PLUGIN	pBasePlugin = FindPlugin(basedir);
	if(!pBasePlugin || !pBasePlugin->not_execute){
		// not_execute should be true!
		return NULL;
	}

	pPlugin				= new K2HFT_PLUGIN;
	pPlugin->BaseParam	= pBasePlugin->BaseParam;
	pPlugin->OutputPath	= outputpath;
	pPlugin->mode		= S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH | S_IFREG;
	pPlugin->not_execute= false;

	// set named pipe file path
	if(!K2hFtPluginMan::BuildNamedPipeFile(pPlugin->PipeFilePath)){
		ERR_K2HFTPRN("Failed to make named pipe file.");
		K2HFT_DELETE(pPlugin);
		return NULL;
	}

	// add & run new plugin
	if(!pluginman.RunAddPlugin(pPlugin)){
		ERR_K2HFTPRN("Internal error: something error occurred during run plugin.");
		K2HFT_DELETE(pPlugin);
		return NULL;
	}

	// add plugin to mapping
	pluginpathmap[outputpath] = pPlugin;

	return pPlugin;
}

/*
 * VIM modelines
 *
 * vim:set ts=4 fenc=utf-8:
 */
