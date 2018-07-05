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

#include <stdlib.h>
#include <strings.h>
#include <errno.h>
#include <signal.h>

#include "k2hftdbg.h"

//---------------------------------------------------------
// Global variable
//---------------------------------------------------------
K2hFtDbgMode	k2hft_debug_mode		= K2HFTDBG_SILENT;
FILE*			k2hft_dbg_fp			= NULL;
bool			k2hft_foreground_mode	= false;

//---------------------------------------------------------
// Class K2hFtDbgCtrl
//---------------------------------------------------------
class K2hFtDbgCtrl
{
	protected:
		static const char*	DBGENVNAME;
		static const char*	DBGENVFILE;

		bool				isSetSignal;
		K2hFtDbgMode*		pk2hft_debug_mode;
		FILE**				pk2hft_dbg_fp;

	protected:
		K2hFtDbgCtrl() : isSetSignal(false), pk2hft_debug_mode(&k2hft_debug_mode), pk2hft_dbg_fp(&k2hft_dbg_fp)
		{
			*pk2hft_debug_mode	= K2HFTDBG_SILENT;
			*pk2hft_dbg_fp		= NULL;
			FtDbgCtrlLoadEnv();
		}

		virtual ~K2hFtDbgCtrl()
		{
			SetFtDbgCtrlSignalUser1(false);
		}

		bool FtDbgCtrlLoadEnvName(void);
		bool FtDbgCtrlLoadEnvFile(void);

	public:
		static K2hFtDbgCtrl& GetK2hFtDbgCtrl(void)
		{
			static K2hFtDbgCtrl	singleton;			// singleton
			return singleton;
		}
		static bool CvtModeString(const char* strmode, K2hFtDbgMode& mode);
		static void User1Handler(int Signal);

		bool FtDbgCtrlLoadEnv(void);
		bool SetFtDbgCtrlSignalUser1(bool isEnable);
		void InitFtDbgCtrlSyslog(void);

		K2hFtDbgMode SetFtDbgCtrlMode(K2hFtDbgMode mode);
		K2hFtDbgMode BumpupFtDbgCtrlMode(void);
		K2hFtDbgMode GetFtDbgCtrlMode(void);
		bool SetFtDbgCtrlModeByString(const char* strmode);
		bool SetFtDbgCtrlFile(const char* filepath);
		bool UnsetFtDbgCtrlFile(void);
};

//
// Class valiables
//
const char*		K2hFtDbgCtrl::DBGENVNAME = "K2HFTDBGMODE";
const char*		K2hFtDbgCtrl::DBGENVFILE = "K2HFTDBGFILE";

//
// Class Methods
//
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

void K2hFtDbgCtrl::User1Handler(int Signal)
{
	MSG_K2HFTPRN("Caught signal SIGUSR1(%d), bumpup the logging level.", Signal);
	K2hFtDbgCtrl::GetK2hFtDbgCtrl().BumpupFtDbgCtrlMode();
}

//
// Methods
//
bool K2hFtDbgCtrl::FtDbgCtrlLoadEnv(void)
{
	if(!FtDbgCtrlLoadEnvName() || !FtDbgCtrlLoadEnvFile()){
		return false;
	}
	return true;
}

bool K2hFtDbgCtrl::FtDbgCtrlLoadEnvName(void)
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
	SetFtDbgCtrlMode(newmode);
	return true;
}

bool K2hFtDbgCtrl::FtDbgCtrlLoadEnvFile(void)
{
	char*	pEnvVal;
	if(NULL == (pEnvVal = getenv(K2hFtDbgCtrl::DBGENVFILE))){
		MSG_K2HFTPRN("%s ENV is not set.", K2hFtDbgCtrl::DBGENVFILE);
		return true;
	}
	if(!SetFtDbgCtrlFile(pEnvVal)){
		MSG_K2HFTPRN("%s ENV is unsafe string(%s).", K2hFtDbgCtrl::DBGENVFILE, pEnvVal);
		return false;
	}
	return true;
}

bool K2hFtDbgCtrl::SetFtDbgCtrlSignalUser1(bool isEnable)
{
	if(isEnable != isSetSignal){
		struct sigaction	sa;

		sigemptyset(&sa.sa_mask);
		sigaddset(&sa.sa_mask, SIGUSR1);
		sa.sa_flags		= isEnable ? 0 : SA_RESETHAND;
		sa.sa_handler	= isEnable ? K2hFtDbgCtrl::User1Handler : SIG_DFL;

		if(0 > sigaction(SIGUSR1, &sa, NULL)){
			WAN_K2HFTPRN("Could not %s signal USER1 handler. errno = %d", isEnable ? "set" : "unset", errno);
			return false;
		}
		isSetSignal = isEnable;
	}
	return true;
}

void K2hFtDbgCtrl::InitFtDbgCtrlSyslog(void)
{
	static bool	is_open = false;

	if(!is_open){
		// set syslog style
		openlog("k2hftfuse", LOG_PID | LOG_ODELAY | LOG_NOWAIT, LOG_USER);
		is_open = true;
	}
	// set syslog level
	setlogmask(LOG_UPTO(CVT_K2HFTDBGMODE_SYSLOGLEVEL(*pk2hft_debug_mode)));
}

K2hFtDbgMode K2hFtDbgCtrl::SetFtDbgCtrlMode(K2hFtDbgMode mode)
{
	K2hFtDbgMode	oldmode	= *pk2hft_debug_mode;
	*pk2hft_debug_mode 		= mode;

	// set syslog level
	InitFtDbgCtrlSyslog();

	return oldmode;
}

bool K2hFtDbgCtrl::SetFtDbgCtrlModeByString(const char* strmode)
{
	K2hFtDbgMode	newmode;
	if(!K2hFtDbgCtrl::CvtModeString(strmode, newmode)){
		MSG_K2HFTPRN("%s is not unknown mode.", strmode ? strmode : "");
		return false;
	}
	SetFtDbgCtrlMode(newmode);
	return true;
}

K2hFtDbgMode K2hFtDbgCtrl::BumpupFtDbgCtrlMode(void)
{
	K2hFtDbgMode	mode = GetFtDbgCtrlMode();

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
	return SetFtDbgCtrlMode(mode);
}

K2hFtDbgMode K2hFtDbgCtrl::GetFtDbgCtrlMode(void)
{
	return *pk2hft_debug_mode;
}

bool K2hFtDbgCtrl::SetFtDbgCtrlFile(const char* filepath)
{
	if(K2HFT_ISEMPTYSTR(filepath)){
		ERR_K2HFTPRN("Parameter is wrong.");
		return false;
	}
	if(!UnsetFtDbgCtrlFile()){
		return false;
	}
	FILE*	newfp;
	if(NULL == (newfp = fopen(filepath, "a+"))){
		ERR_K2HFTPRN("Could not open debug file(%s). errno = %d", filepath, errno);
		return false;
	}
	*pk2hft_dbg_fp = newfp;
	return true;
}

bool K2hFtDbgCtrl::UnsetFtDbgCtrlFile(void)
{
	if(*pk2hft_dbg_fp){
		if(0 != fclose(*pk2hft_dbg_fp)){
			ERR_K2HFTPRN("Could not close debug file. errno = %d", errno);
			*pk2hft_dbg_fp = NULL;		// On this case, k2hft_dbg_fp is not correct pointer after error...
			return false;
		}
		*pk2hft_dbg_fp = NULL;
	}
	return true;
}

//---------------------------------------------------------
// Global Functions
//---------------------------------------------------------
void InitK2hFtDbgSyslog(void)
{
	K2hFtDbgCtrl::GetK2hFtDbgCtrl().InitFtDbgCtrlSyslog();
}

K2hFtDbgMode SetK2hFtDbgMode(K2hFtDbgMode mode)
{
	return K2hFtDbgCtrl::GetK2hFtDbgCtrl().SetFtDbgCtrlMode(mode);
}

bool SetK2hFtDbgModeByString(const char* strmode)
{
	return K2hFtDbgCtrl::GetK2hFtDbgCtrl().SetFtDbgCtrlModeByString(strmode);
}

K2hFtDbgMode BumpupK2hFtDbgMode(void)
{
	return K2hFtDbgCtrl::GetK2hFtDbgCtrl().BumpupFtDbgCtrlMode();
}

K2hFtDbgMode GetK2hFtDbgMode(void)
{
	return K2hFtDbgCtrl::GetK2hFtDbgCtrl().GetFtDbgCtrlMode();
}

bool LoadK2hFtDbgEnv(void)
{
	return K2hFtDbgCtrl::GetK2hFtDbgCtrl().FtDbgCtrlLoadEnv();
}

bool SetK2hFtDbgFile(const char* filepath)
{
	return K2hFtDbgCtrl::GetK2hFtDbgCtrl().SetFtDbgCtrlFile(filepath);
}

bool UnsetK2hFtDbgFile(void)
{
	return K2hFtDbgCtrl::GetK2hFtDbgCtrl().UnsetFtDbgCtrlFile();
}

bool SetK2hFtSignalUser1(void)
{
	return K2hFtDbgCtrl::GetK2hFtDbgCtrl().SetFtDbgCtrlSignalUser1(true);
}

/*
 * VIM modelines
 *
 * vim:set ts=4 fenc=utf-8:
 */
