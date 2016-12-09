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
 * CREATE:   Wed Sep 2 2015
 * REVISION:
 *
 */

#include <stdlib.h>
#include <strings.h>
#include <errno.h>
#include <signal.h>

#include "k2hftdbg.h"

//---------------------------------------------------------
// Class K2hFtDbgCtrl
//---------------------------------------------------------
class K2hFtDbgCtrl
{
	protected:
		static const char*	DBGENVNAME;
		static const char*	DBGENVFILE;
		static K2hFtDbgCtrl	singleton;
		static bool			isSetSignal;

	public:
		static bool CvtModeString(const char* strmode, K2hFtDbgMode& mode);
		static bool LoadEnv(void);
		static bool LoadEnvName(void);
		static bool LoadEnvFile(void);
		static void User1Handler(int Signal);
		static bool SetK2hFtSignalUser1(bool isEnable);

		K2hFtDbgCtrl();
		virtual ~K2hFtDbgCtrl();
};

// Class valiables
const char*		K2hFtDbgCtrl::DBGENVNAME = "K2HFTDBGMODE";
const char*		K2hFtDbgCtrl::DBGENVFILE = "K2HFTDBGFILE";
K2hFtDbgCtrl	K2hFtDbgCtrl::singleton;
bool			K2hFtDbgCtrl::isSetSignal=	false;

// Constructor / Destructor
K2hFtDbgCtrl::K2hFtDbgCtrl()
{
	K2hFtDbgCtrl::LoadEnv();
}
K2hFtDbgCtrl::~K2hFtDbgCtrl()
{
	K2hFtDbgCtrl::SetK2hFtSignalUser1(false);
}

// Class Methods
bool K2hFtDbgCtrl::CvtModeString(const char* strmode, K2hFtDbgMode& mode)
{
	if(K2HFT_ISEMPTYSTR(strmode)){
		return false;
	}
	if(0 == strcasecmp(strmode, "SILENT")){
		mode = K2HFTDBG_SILENT;
	}else if(0 == strcasecmp(strmode, "ERR") || 0 == strcasecmp(strmode, "ERROR")){
		mode = K2HFTDBG_ERR;
	}else if(0 == strcasecmp(strmode, "WAN") || 0 == strcasecmp(strmode, "WARN") || 0 == strcasecmp(strmode, "WARNING")){
		mode = K2HFTDBG_WARN;
	}else if(0 == strcasecmp(strmode, "INF") || 0 == strcasecmp(strmode, "INFO") || 0 == strcasecmp(strmode, "INFORMATION")){
		mode = K2HFTDBG_MSG;
	}else if(0 == strcasecmp(strmode, "MSG") || 0 == strcasecmp(strmode, "MESSAGE")){
		mode = K2HFTDBG_MSG;
	}else if(0 == strcasecmp(strmode, "DMP") || 0 == strcasecmp(strmode, "DUMP")){
		mode = K2HFTDBG_DUMP;
	}else{
		return false;
	}
	return true;
}

bool K2hFtDbgCtrl::LoadEnv(void)
{
	if(!K2hFtDbgCtrl::LoadEnvName() || !K2hFtDbgCtrl::LoadEnvFile()){
		return false;
	}
	return true;
}

bool K2hFtDbgCtrl::LoadEnvName(void)
{
	char*	pEnvVal;
	if(NULL == (pEnvVal = getenv(K2hFtDbgCtrl::DBGENVNAME))){
		MSG_K2HFTPRN("%s ENV is not set.", K2hFtDbgCtrl::DBGENVNAME);
		return true;
	}
	K2hFtDbgMode	newmode;
	if(!K2hFtDbgCtrl::CvtModeString(pEnvVal, newmode)){
		MSG_K2HFTPRN("%s ENV is not unknown string(%s).", K2hFtDbgCtrl::DBGENVNAME, pEnvVal);
		return false;
	}
	SetK2hFtDbgMode(newmode);
	return true;
}

bool K2hFtDbgCtrl::LoadEnvFile(void)
{
	char*	pEnvVal;
	if(NULL == (pEnvVal = getenv(K2hFtDbgCtrl::DBGENVFILE))){
		MSG_K2HFTPRN("%s ENV is not set.", K2hFtDbgCtrl::DBGENVFILE);
		return true;
	}
	if(!SetK2hFtDbgFile(pEnvVal)){
		MSG_K2HFTPRN("%s ENV is unsafe string(%s).", K2hFtDbgCtrl::DBGENVFILE, pEnvVal);
		return false;
	}
	return true;
}

void K2hFtDbgCtrl::User1Handler(int Signal)
{
	MSG_K2HFTPRN("Caught signal SIGUSR1(%d), bumpup the logging level.", Signal);
	BumpupK2hFtDbgMode();
}

bool K2hFtDbgCtrl::SetK2hFtSignalUser1(bool isEnable)
{
	if(isEnable != K2hFtDbgCtrl::isSetSignal){
		struct sigaction	sa;

		sigemptyset(&sa.sa_mask);
		sigaddset(&sa.sa_mask, SIGUSR1);
		sa.sa_flags		= isEnable ? 0 : SA_RESETHAND;
		sa.sa_handler	= isEnable ? K2hFtDbgCtrl::User1Handler : SIG_DFL;

		if(0 > sigaction(SIGUSR1, &sa, NULL)){
			WAN_K2HFTPRN("Could not %s signal USER1 handler. errno = %d", isEnable ? "set" : "unset", errno);
			return false;
		}
		K2hFtDbgCtrl::isSetSignal = isEnable;
	}
	return true;
}

//---------------------------------------------------------
// Global variable
//---------------------------------------------------------
K2hFtDbgMode	k2hft_debug_mode		= K2HFTDBG_SILENT;
FILE*			k2hft_dbg_fp			= NULL;
bool			k2hft_foreground_mode	= false;

void InitK2hFtDbgSyslog(void)
{
	static bool	is_open = false;

	if(!is_open){
		// set syslog style
		openlog("k2hftfuse", LOG_PID | LOG_ODELAY | LOG_NOWAIT, LOG_USER);
		is_open = true;
	}
	// set syslog level
	setlogmask(LOG_UPTO(CVT_K2HFTDBGMODE_SYSLOGLEVEL(k2hft_debug_mode)));
}

K2hFtDbgMode SetK2hFtDbgMode(K2hFtDbgMode mode)
{
	K2hFtDbgMode	oldmode	= k2hft_debug_mode;
	k2hft_debug_mode 		= mode;

	// set syslog level
	InitK2hFtDbgSyslog();

	return oldmode;
}

bool SetK2hFtDbgModeByString(const char* strmode)
{
	K2hFtDbgMode	newmode;
	if(!K2hFtDbgCtrl::CvtModeString(strmode, newmode)){
		MSG_K2HFTPRN("%s is not unknown mode.", strmode ? strmode : "");
		return false;
	}
	SetK2hFtDbgMode(newmode);
	return true;
}

K2hFtDbgMode BumpupK2hFtDbgMode(void)
{
	K2hFtDbgMode	mode = GetK2hFtDbgMode();

	if(K2HFTDBG_SILENT == mode){
		mode = K2HFTDBG_ERR;
	}else if(K2HFTDBG_ERR == mode){
		mode = K2HFTDBG_WARN;
	}else if(K2HFTDBG_WARN == mode){
		mode = K2HFTDBG_MSG;
	}else if(K2HFTDBG_MSG == mode){
		mode = K2HFTDBG_DUMP;
	}else{	// K2HFTDBG_DUMP == mode
		mode = K2HFTDBG_SILENT;
	}
	return ::SetK2hFtDbgMode(mode);
}

K2hFtDbgMode GetK2hFtDbgMode(void)
{
	return k2hft_debug_mode;
}

bool LoadK2hFtDbgEnv(void)
{
	return K2hFtDbgCtrl::LoadEnv();
}

bool SetK2hFtDbgFile(const char* filepath)
{
	if(K2HFT_ISEMPTYSTR(filepath)){
		ERR_K2HFTPRN("Parameter is wrong.");
		return false;
	}
	if(!UnsetK2hFtDbgFile()){
		return false;
	}
	FILE*	newfp;
	if(NULL == (newfp = fopen(filepath, "a+"))){
		ERR_K2HFTPRN("Could not open debug file(%s). errno = %d", filepath, errno);
		return false;
	}
	k2hft_dbg_fp = newfp;
	return true;
}

bool UnsetK2hFtDbgFile(void)
{
	if(k2hft_dbg_fp){
		if(0 != fclose(k2hft_dbg_fp)){
			ERR_K2HFTPRN("Could not close debug file. errno = %d", errno);
			k2hft_dbg_fp = NULL;		// On this case, k2hft_dbg_fp is not correct pointer after error...
			return false;
		}
		k2hft_dbg_fp = NULL;
	}
	return true;
}

bool SetK2hFtSignalUser1(void)
{
	return K2hFtDbgCtrl::SetK2hFtSignalUser1(true);
}

/*
 * VIM modelines
 *
 * vim:set ts=4 fenc=utf-8:
 */
