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
 * CREATE:   Tue Sep 1 2015
 * REVISION:
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

#include <k2hash/k2hash.h>
#include <k2hash/k2hcommand.h>
#include <k2hash/k2htransfunc.h>
#include <chmpx/chmpx.h>
#include <chmpx/chmcntrl.h>
#include <chmpx/chmconfutil.h>

#include <map>

#include "k2hftcommon.h"
#include "k2hftsvrinfo.h"
#include "k2hftstructure.h"
#include "k2hftutil.h"
#include "k2hftdbg.h"

using namespace std;

//---------------------------------------------------------
// Symbol
//---------------------------------------------------------
#define	K2HFTSVR_EVENT_WAIT_TIMEMS				1000			// 1s

// Environment
#define	K2HFTSVR_CONFFILE_ENV_NAME				"K2HFTSVRCONFFILE"
#define	K2HFTSVR_JSONCONF_ENV_NAME				"K2HFTSVRJSONCONF"

//---------------------------------------------------------
// Structure
//---------------------------------------------------------
//
// For option parser
//
typedef struct opt_param{
	std::string		rawstring;
	bool			is_number;
	int				num_value;
}OPTPARAM, *POPTPARAM;

typedef std::map<std::string, OPTPARAM>		optparams_t;

//---------------------------------------------------------
// Variable
//---------------------------------------------------------
static volatile bool	is_doing_loop = true;

//---------------------------------------------------------
// Version
//---------------------------------------------------------
extern char k2hftfusesvr_commit_hash[];

static void k2hftfusesvr_print_version(FILE* stream)
{
	static const char format[] =
		"\n"
		"k2hftfusesvr Version %s (commit: %s)\n"
		"\n"
		"Copyright 2015 Yahoo! JAPAN corporation.\n"
		"\n"
		"k2hftfusesvr is file transaction receiver server on FUSE file\n"
		"system with K2HASH and K2HASH TRANSACTION PLUGIN, CHMPX.\n"
		"\n";

	if(!stream){
		stream = stdout;
	}
	fprintf(stream, format, VERSION, k2hftfusesvr_commit_hash);

	k2h_print_version(stream);
	chmpx_print_version(stream);
}

static void k2hftfusesvr_print_usage(FILE* stream)
{
	if(!stream){
		stream = stdout;
	}
	fprintf(stream,	"[Usage] k2hftfusesvr\n"
					"\n"
					"  k2hftfusesvr [k2hftfusesvr or fuse options]\n"
					"\n"
					" * k2hftfusesvr options:\n"
					"     -h(help)                     print help\n"
					"     -v(version)                  print version\n"
					"     -conf <configration file>    specify configration file(.ini .yaml .json) for k2hftfusesvr and all sub system\n"
					"     -json <json string>          specify configration json string for k2hftfusesvr and all sub system\n"
					"     -d(g) {err|wan|msg|silent}   set debug level affect only k2hftfusesvr\n"
					"\n"
					" * k2hftfusesvr environments:\n"
					"     K2HFTSVRCONFFILE             specify configration file(.ini .yaml .json) instead of conf option.\n"
					"     K2HFTSVRJSONCONF             specify json string as configration instead of json option.\n"
					"\n"
					" Please see man page - k2hftfusesvr(1) for more details.\n"
					"\n"	);
}

static bool parse_parameter(int argc, char** argv, optparams_t& optparams)
{
	if(argc < 2 || !argv){
		ERR_K2HFTPRN("no parameters.");
		return false;
	}
	optparams.clear();

	for(int cnt = 1; cnt < argc && argv && argv[cnt]; cnt++){
		OPTPARAM	param;
		param.rawstring = "";
		param.is_number = false;
		param.num_value = 0;

		// get option name
		char*	popt = argv[cnt];
		if(K2HFT_ISEMPTYSTR(popt)){
			continue;		// skip
		}
		if('-' != *popt){
			ERR_K2HFTPRN("%s option is not started with \"-\".", popt);
			continue;
		}

		// check option parameter
		if((cnt + 1) < argc && argv[cnt + 1]){
			char*	pparam = argv[cnt + 1];
			if(!K2HFT_ISEMPTYSTR(pparam) && '-' != *pparam){
				// found param
				param.rawstring = pparam;

				// check number
				param.is_number = true;
				for(char* ptmp = pparam; *ptmp; ++ptmp){
					if(0 == isdigit(*ptmp)){
						param.is_number = false;
						break;
					}
				}
				if(param.is_number){
					param.num_value = atoi(pparam);
				}
				++cnt;
			}
		}
		optparams[lower(string(popt))] = param;
	}
	return true;
}

//---------------------------------------------------------
// Parser
//---------------------------------------------------------
static bool ParseBinCom(PBCOM pBinCom, unsigned char** ppKey, size_t& keylen, unsigned char** ppVal, size_t& vallen)
{
	if(!pBinCom){
		ERR_K2HFTPRN("pBinCom is NULL.");
		return false;
	}
	// check type
	if(SCOM_SET_ALL != pBinCom->scom.type && SCOM_REPLACE_VAL != pBinCom->scom.type && SCOM_OW_VAL != pBinCom->scom.type){
		// [NOTE]
		// Often come here because removing key makes DEL_KEY tansaction.
		// REP_ATTRS should not occur, because attributes are not changed on this system.
		//
		//MSG_K2HFTPRN("pBinCom data type(%ld: %s) is not k2hftfusesvr target type.", pBinCom->scom.type, pBinCom->scom.szCommand);
		return false;
	}
	// check data and set buffer
	if(0 == pBinCom->scom.key_length || 0 == pBinCom->scom.val_length){
		WAN_K2HFTPRN("pBinCom doe not have key or value.");
		return false;
	}
	*ppKey	= static_cast<unsigned char*>(&pBinCom->byData[pBinCom->scom.key_pos]);
	*ppVal	= static_cast<unsigned char*>(&pBinCom->byData[pBinCom->scom.val_pos]);
	keylen	= pBinCom->scom.key_length;
	vallen	= pBinCom->scom.val_length;

	return true;
}

//---------------------------------------------------------
// Utility
//---------------------------------------------------------
static K2HShm* MakeTransK2h(K2hFtSvrInfo& confinfo)
{
	if(!confinfo.IsTransMode()){
		ERR_K2HFTPRN("ConfInfo does not transfer mode.");
		return NULL;
	}

	// attach k2hash for output transfer
	K2HShm*	pTransK2h = new K2HShm();

	// set thread pool count
	if(!K2HShm::SetTransThreadPool(confinfo.GetThreadCntTransK2h())){
		ERR_K2HFTPRN("Failed to set transaction thread pool(count=%d).", confinfo.GetThreadCntTransK2h());
		K2HFT_DELETE(pTransK2h);
		return NULL;
	}

	// make k2hash
	bool	bresult = false;
	if(confinfo.IsMemoryTypeTransK2h()){
		// memory type
		bresult = pTransK2h->AttachMem(confinfo.GetMaskTransK2h(), confinfo.GetCMaskTransK2h(), confinfo.GetElementCntTransK2h(), confinfo.GetPageSizeTransK2h());
	}else{
		// file type
		if(confinfo.IsInitializeTransK2h()){
			// if file exists, remove it.
			if(!remove_exist_file(confinfo.GetPathTransK2h())){
				ERR_K2HFTPRN("Found file(%s), but could not initialize it.", confinfo.GetPathTransK2h());
				bresult = false;
			}else{
				bresult = pTransK2h->Attach(confinfo.GetPathTransK2h(), false, true, false, confinfo.IsFullmapTransK2h(), confinfo.GetMaskTransK2h(), confinfo.GetCMaskTransK2h(), confinfo.GetElementCntTransK2h(), confinfo.GetPageSizeTransK2h());
			}
		}else{
			bresult = pTransK2h->Attach(confinfo.GetPathTransK2h(), false, true, false, confinfo.IsFullmapTransK2h(), confinfo.GetMaskTransK2h(), confinfo.GetCMaskTransK2h(), confinfo.GetElementCntTransK2h(), confinfo.GetPageSizeTransK2h());
		}
	}
	if(!bresult){
		ERR_K2HFTPRN("Could not build internal transfer k2hash file(memory) for k2hftfusesvr.");
		K2HFT_DELETE(pTransK2h);
		return NULL;
	}

	// load k2htpdtor
	if(!K2HTransDynLib::get()->Load(confinfo.GetCtpDtorPath())){
		ERR_K2HFTPRN("Could not load %s transaction plugin.", confinfo.GetCtpDtorPath());
		pTransK2h->Detach();
		K2HFT_DELETE(pTransK2h);
		return NULL;
	}

	// enable transaction plugin
	if(!pTransK2h->EnableTransaction(NULL, NULL, 0L, reinterpret_cast<const unsigned char*>(confinfo.GetConfigTransK2h()), strlen(confinfo.GetConfigTransK2h()) + 1)){
		ERR_K2HFTPRN("Could not enable %s transaction plugin.", confinfo.GetCtpDtorPath());
		ERR_K2HFTPRN("Hint! Please check chmpx slave process running, if it does not run, k2hftfuse can not run.");
		K2HTransDynLib::get()->Unload();
		pTransK2h->Detach();
		K2HFT_DELETE(pTransK2h);
		return NULL;
	}
	return pTransK2h;
}

//---------------------------------------------------------
// Signal handler
//---------------------------------------------------------
static void SigHuphandler(int signum)
{
	if(SIGHUP == signum){
		is_doing_loop = false;
	}
}

//---------------------------------------------------------
// Processing
//---------------------------------------------------------
bool Processing(K2hFtSvrInfo& confinfo, K2hFtPluginMan& pluginman, K2HShm* pTransK2h, K2hFtFdCache& fdcache, bool IsUnityMode, string& OutputPath, unsigned char* pbody, size_t length)
{
	if(!pbody){
		ERR_K2HFTPRN("Body data is empty.");
		return false;
	}

	// parse body to key & value
	unsigned char*	pKey	= NULL;
	unsigned char*	pVal	= NULL;
	size_t			keylen	= 0;
	size_t			vallen	= 0;
	{
		PBCOM	pBinCom = reinterpret_cast<PBCOM>(pbody);
		if(length != scom_total_length(pBinCom->scom)){
			ERR_K2HFTPRN("Length(%zu) of recieved message body is wrong(should be %zu).", length, scom_total_length(pBinCom->scom));
			return false;
		}
		if(!ParseBinCom(pBinCom, &pKey, keylen, &pVal, vallen)){
			// [NOTE]
			// Often come here because removing key makes DEL_KEY tansaction.
			// Because of the suppression of the output of the error message, returns true.
			//
			//MSG_K2HFTPRN("Received data is not target type, or does not have key or value, or something wrong occurred.");
			return true;
		}
	}

	// output file mode
	if(confinfo.IsOutputFileMode()){
		PK2HFTVALUE		pk2hftval	= reinterpret_cast<PK2HFTVALUE>(pVal);
		size_t			count		= 0;
		PK2HFTLINE		pLines		= GetK2hFtLines(pk2hftval, &count);
		char*			filepath	= NULL;
		char*			pfusepid	= NULL;

		// get filepath & pid
		if(!ParseK2hFtKey(pKey, &filepath, &pfusepid) || !filepath || !pfusepid){
			ERR_K2HFTPRN("Could not parse received key and value to some parts.");

		}else{
			unsigned char*	pFileOutput = NULL;			// stacking for putting file
			size_t			FilePutSize	= 0;
			string			OutputFile;

			// make target file path & plugin
			if(IsUnityMode){
				OutputFile = OutputPath;
			}else{
				OutputFile = OutputPath + filepath;
			}

			// loop for line count
			for(size_t cnt = 0; cnt < count && pLines; ++cnt){
				PK2HFTLINE		pNextLines	= GetNextK2hFtLine(pLines);

				// perse
				unsigned char*	data		= NULL;
				size_t			datalength	= 0;
				char*			hostname	= NULL;
				struct timespec	ts			= {0, 0};
				pid_t			pid			= K2HFT_INVALID_PID;
				string			cvtoutput;				// temporary for converted output
				if(!ParseK2hFtLine(pLines, &data, &datalength, &hostname, &ts, &pid) || !data || 0 == datalength || !hostname){
					ERR_K2HFTPRN("Could not parse received key and value to some parts, but continue...");
					continue;
				}

				// for debugging
				if(IS_K2HFTDBG_DUMP()){
					K2HFTPRN("Received data = {");
					K2HFTPRN("  file path         : %s",	filepath ? filepath : "null");
					K2HFTPRN("  fuse pid          : %s",	pfusepid);
					K2HFTPRN("  data = {");
					K2HFTPRN("    %s",						reinterpret_cast<char*>(data));
					K2HFTPRN("  }");
					K2HFTPRN("  data length       : %zu",	datalength);
					K2HFTPRN("  hostname          : %s",	hostname);
					K2HFTPRN("  client pid        : %d",	pid);
					K2HFTPRN("  time spec         : %zds %09ldns", ts.tv_sec, ts.tv_nsec);
					K2HFTPRN("}");
				}

				// convert output data
				if(confinfo.IsNeedConvert()){
					// convert output
					if(!confinfo.ConvertOutput(cvtoutput, hostname, pid, filepath, &ts, reinterpret_cast<const char*>(data)) || cvtoutput.empty()){
						ERR_K2HFTPRN("Failed to convert output data.");
						continue;
					}
					// set pointer etc
					data		= const_cast<unsigned char*>(reinterpret_cast<const unsigned char*>(cvtoutput.c_str()));
					datalength	= cvtoutput.length();
				}

				// put data
				if(!confinfo.IsPluginEmpty()){
					// get plugin (or make new plugin and run)
					PK2HFT_PLUGIN	pPlugin;
					if(NULL == (pPlugin = confinfo.GetPlugin(OutputFile, OutputPath, pluginman))){
						ERR_K2HFTPRN("Not running plugin for %s.", OutputFile.c_str());
					}else{
						// put to plugin
						if(!pluginman.Write(pPlugin, data, ((!confinfo.IsBinMode() && 0x00 == data[datalength - 1]) ? (datalength - 1) : datalength))){
							ERR_K2HFTPRN("Could not write to plugin input pipe.");
						}
					}

				}else{
					// put file, do stacking
					size_t	NewSize = FilePutSize + ((!confinfo.IsBinMode() && 0x00 == data[datalength - 1]) ? (datalength - 1) : datalength);

					if(NULL == (pFileOutput = reinterpret_cast<unsigned char*>(realloc(pFileOutput, NewSize)))){
						ERR_K2HFTPRN("Could not allocate memory, but continue...");
					}else{
						memcpy(&pFileOutput[FilePutSize], data, ((!confinfo.IsBinMode() && 0x00 == data[datalength - 1]) ? (datalength - 1) : datalength));
						FilePutSize = NewSize;
					}
				}

				pLines = pNextLines;
			}

			// put to file at HERE!
			if(pFileOutput){
				// put to file
				if(!fdcache.Write(OutputFile, O_WRONLY | O_APPEND | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH, pFileOutput, FilePutSize)){
					ERR_K2HFTPRN("Could not write to output file(%s).", OutputFile.c_str());
				}
				K2HFT_FREE(pFileOutput);
			}
		}
	}

	// transfer mode
	if(confinfo.IsTransMode()){
		// write trans k2hash
		if(!pTransK2h->Set(pKey, keylen, pVal, vallen, NULL, true)){
			if(IS_K2HFTDBG_DUMP()){
				// parse key
				char*	filepath = NULL;
				char*	pfusepid = NULL;
				if(ParseK2hFtKey(pKey, &filepath, &pfusepid)){
					ERR_K2HFTPRN("Failed to transfer received data(file path=%s, pid=%s).", filepath ? filepath : "null", pfusepid ? pfusepid : "null");
				}else{
					ERR_K2HFTPRN("Failed to transfer received data(could not print detail for dump mode because could not parse key).");
				}
			}else{
				ERR_K2HFTPRN("Failed to transfer received data.");
			}
		}
	}
	return true;
}

//---------------------------------------------------------
// Main
//---------------------------------------------------------
int main(int argc, char** argv)
{
	// always forgroud mode
	k2hft_foreground_mode = true;

	// parse parameter
	optparams_t	optparams;
	if(!parse_parameter(argc, argv, optparams)){
		K2HFTERR("There is no parameter, please run with \"-help\" or see man page, you can see help.");
		exit(EXIT_FAILURE);
	}
	// -h / -help
	if(optparams.end() != optparams.find("-h") || optparams.end() != optparams.find("-help")){
		k2hftfusesvr_print_usage(stdout);
		exit(EXIT_SUCCESS);
	}
	// -v / -ver / -version
	if(optparams.end() != optparams.find("-v") || optparams.end() != optparams.find("-ver") || optparams.end() != optparams.find("-version")){
		k2hftfusesvr_print_version(stdout);
		exit(EXIT_SUCCESS);
	}

	// -g / -d / -dbg
	optparams_t::const_iterator	paramiter;
	if(optparams.end() != (paramiter = optparams.find("-g")) || optparams.end() != (paramiter = optparams.find("-d")) || optparams.end() != (paramiter = optparams.find("-dbg"))){
		if(!SetK2hFtDbgModeByString(paramiter->second.rawstring.c_str())){
			K2HFTERR("Option \"-d(-g)\" has wrong paramter(%s)", paramiter->second.rawstring.c_str());
			exit(EXIT_FAILURE);
		}
	}else{
		SetK2hFtDbgMode(K2HFTDBG_SILENT);
	}

	// -conf / -json / environment
	string	config;
	if(optparams.end() != (paramiter = optparams.find("-conf"))){
		config = paramiter->second.rawstring.c_str();
	}else if(optparams.end() != (paramiter = optparams.find("-json"))){
		config = paramiter->second.rawstring.c_str();
	}else{
		CHMCONFTYPE	conftye = check_chmconf_type_ex(NULL, K2HFTSVR_CONFFILE_ENV_NAME, K2HFTSVR_JSONCONF_ENV_NAME, &config);
		if(CHMCONF_TYPE_UNKNOWN == conftye || CHMCONF_TYPE_NULL == conftye){
			K2HFTERR("Option \"-conf\" or \"-json\" or environments for configurarion must specify for configuration.");
			exit(EXIT_FAILURE);
		}
	}

	// initialize fd cache object
	K2hFtFdCache	fdcache;				// fd cache class
	if(!fdcache.Initialize()){
		ERR_K2HFTPRN("Could not initialize fd cache object.");
		exit(EXIT_FAILURE);
	}

	// initialize plugin manager
	K2hFtPluginMan	pluginman;				// plugin manager
	if(!pluginman.Initialize(&fdcache)){
		ERR_K2HFTPRN("Could not initialize plugin manager.");
		exit(EXIT_FAILURE);
	}

	// load
	K2hFtSvrInfo	confinfo;
	if(!confinfo.Load(config.c_str(), pluginman)){
		K2HFTERR("Faild to load configuration(%s), somethig wrong in file.", config.c_str());
		exit(EXIT_FAILURE);
	}

	// for debugging
	if(IS_K2HFTDBG_DUMP()){
		confinfo.Dump();
	}

	// set exit signal
	struct sigaction	sa;
	sigemptyset(&sa.sa_mask);
	sigaddset(&sa.sa_mask, SIGHUP);
	sa.sa_flags		= 0;
	sa.sa_handler	= SigHuphandler;
	if(0 > sigaction(SIGHUP, &sa, NULL)){
		K2HFTERR("Faild to set signal HUP handler, errno(%d)", errno);
		exit(EXIT_FAILURE);
	}

	// join chmpx(server mode for receiving)
	ChmCntrl	ChmpxServer;
	if(!ChmpxServer.InitializeOnServer(confinfo.GetConfigration(), false)){
		K2HFTERR("Could not join chmpx server, configuration file is %s.", confinfo.GetConfigration());
		exit(EXIT_FAILURE);
	}

	// attach k2hash for output transfer
	K2HShm*		pTransK2h = NULL;
	if(confinfo.IsTransMode()){
		if(NULL == (pTransK2h = MakeTransK2h(confinfo))){
			exit(EXIT_FAILURE);
		}
	}

	// run plugin
	if(!pluginman.IsEmpty()){
		if(!pluginman.RunPlugins()){
			K2HFTERR("Could not join chmpx server, configuration file is %s.", confinfo.GetConfigration());
			exit(EXIT_FAILURE);
		}
	}

	// static data in loop for output file mode.
	bool	IsUnityMode = confinfo.IsUnityFileMode();
	string	OutputPath	= IsUnityMode ? confinfo.GetUnityFilePath() : confinfo.GetOutputBaseDir();

	// main loop
	while(is_doing_loop){
		// receive
		PCOMPKT			pComPkt	= NULL;
		unsigned char*	pbody	= NULL;
		size_t			length	= 0;
		if(!ChmpxServer.Receive(&pComPkt, &pbody, &length, K2HFTSVR_EVENT_WAIT_TIMEMS, true)){
			WAN_K2HFTPRN("Something error occurred during receiving data.");
			if(ChmpxServer.IsChmpxExit()){
				K2HFTERR("chmpx server exited.");
				break;
			}
			continue;
		}

		// check received data
		if(!pComPkt){
			MSG_K2HFTPRN("Timeouted %dms for waiting message, so continue to waiting.", K2HFTSVR_EVENT_WAIT_TIMEMS);
			continue;
		}

		if(!pbody){
			MSG_K2HFTPRN("NULL body data recieved.");
			K2HFT_FREE(pComPkt);
			continue;
		}
		K2HFT_FREE(pComPkt);
		//MSG_K2HFTPRN("Succeed to receive message.");

		// processing
		if(!Processing(confinfo, pluginman, pTransK2h, fdcache, IsUnityMode, OutputPath, pbody, length)){
			MSG_K2HFTPRN("Failed to processing, but continue...");
		}
		K2HFT_FREE(pbody);
	}

	// Stop plugins
	//
	// [NOTICE]
	// Must stop all plugins before K2hFtSvrInfo is destructed.
	// K2hFtPluginMan has all plugin objects which are pointers, the pointers
	// are freed at K2hFtSvrInfo class destructor.
	//
	if(!pluginman.IsEmpty()){
		if(!pluginman.Clean()){
			ERR_K2HFTPRN("Failed to stop plugins, but continue...");
		}
	}

	// destroy fd cache
	if(!fdcache.Close()){
		ERR_K2HFTPRN("Failed to destroy fd cache object, but continue...");
	}

	if(confinfo.IsTransMode() && !K2HTransDynLib::get()->Unload()){
		ERR_K2HFTPRN("Failed to unload %s transaction plugin, but continue...", confinfo.GetCtpDtorPath());
	}

	exit(EXIT_SUCCESS);
}

/*
 * VIM modelines
 *
 * vim:set ts=4 fenc=utf-8:
 */
