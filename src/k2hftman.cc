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
#include <unistd.h>
#include <sys/types.h>
#include <assert.h>
#include <k2hash/k2htransfunc.h>
#include <fullock/flckstructure.h>
#include <fullock/flckbaselist.tcc>

#include <map>

#include "k2hftcommon.h"
#include "k2hftman.h"
#include "k2hftutil.h"
#include "k2hftdbg.h"

using namespace std;

//---------------------------------------------------------
// Symbols
//---------------------------------------------------------
#define	K2HFT_TEMPK2H_PATH_PREFIX			"/tmp/k2hftfuse_"
#define	K2HFT_TEMPK2H_PATH_SUFFIX			".k2h"
#define	K2HFT_CHMPX_COMMAND_PART1			"chmpx -conf "
#define	K2HFT_CHMPX_COMMAND_PART2			" -d silent >> "
#define	K2HFT_CHMPX_COMMAND_PART3			" 2>&1 &"

//---------------------------------------------------------
// Map for K2HFTVALUE
//---------------------------------------------------------
typedef std::map<uint64_t, PK2HFTVALUE>		k2hftvaluemap_t;

//---------------------------------------------------------
// Class Method
//---------------------------------------------------------
// [NOTE]
// To avoid static object initialization order problem(SIOF)
//
string& K2hFtManage::GetMountPointString(void)
{
	static std::string	mount_point;				// singleton
	return mount_point;
}

const char* K2hFtManage::GetMountPoint(void)
{
	return K2hFtManage::GetMountPointString().c_str();
}

bool K2hFtManage::SetMountPoint(const char* path)
{
	if(K2HFT_ISEMPTYSTR(path) || '/' != path[0]){
		ERR_K2HFTPRN("path(%s) does not mount point.", path ? path : "null");
		return false;
	}
	K2hFtManage::GetMountPointString() = path;
	return true;
}

//
// [NOTE]
// This function is worker thread process for checking timeup data.
//
void* K2hFtManage::TimeupWorkerProc(void* param)
{
	K2hFtManage*	pFtMan = reinterpret_cast<K2hFtManage*>(param);
	time_t			Timeup = K2hFtWriteBuff::GetStackTimeup();

	// check
	if(!pFtMan || Timeup <= 0){
		ERR_K2HFTPRN("Could not run timeup worker thread(%lu), parameter is wrong or timeup is zero.", pthread_self());
		pthread_exit(NULL);
	}
	MSG_K2HFTPRN("Timeup Worker thread(%ld) start up now.", pthread_self());

	struct timespec	sleeptime	= {0, 100 * 1000 * 1000};				// 100ms
	size_t			sleepcnt	= static_cast<size_t>(Timeup * 10);		// timeup(s) / 100ms
	k2hftvaluemap_t	valmap;

	while(pFtMan->run_thread){
		// wait for timeup
		for(size_t cnt = 0; cnt < sleepcnt && pFtMan->run_thread; ++cnt){
			nanosleep(&sleeptime, NULL);
		}
		// cppcheck-suppress knownConditionTrueFalse
		if(!pFtMan->run_thread){
			break;
		}

		// check all buffer & stacking
		while(!fullock::flck_trylock_noshared_mutex(&(pFtMan->wstack_lockval)));	// LOCK
		for(k2hftwbufmap_t::iterator iter = pFtMan->wbstackmap.begin(); iter != pFtMan->wbstackmap.end() && pFtMan->run_thread; ++iter){
			uint64_t		filehandle	= iter->first;
			K2hFtWriteBuff*	pwbuf		= iter->second;

			// check timeup
			if(pwbuf && pwbuf->IsStackLimit()){
				// get stacked data formatted by k2hash value.
				PK2HFTVALUE	pValue = pwbuf->Pop();
				if(pValue){
					// stacked
					valmap[filehandle] = pValue;
				}
			}
		}
		fullock::flck_unlock_noshared_mutex(&(pFtMan->wstack_lockval));				// UNLOCK

		// Do processing...
		for(k2hftvaluemap_t::iterator iter = valmap.begin(); iter != valmap.end(); valmap.erase(iter++)){
			uint64_t	filehandle	= iter->first;
			PK2HFTVALUE	pValue		= iter->second;

			if(false == pFtMan->confinfo.Processing(&(pFtMan->k2hash), pValue, filehandle, pFtMan->fdcache, pFtMan->pluginman)){
				ERR_K2HFTPRN("Something error occurred during processing in timeup thread, skip it.");
			}
			K2HFT_FREE(pValue);
		}
	}
	MSG_K2HFTPRN("Timeup Worker thread(%ld) exit now.", pthread_self());
	pthread_exit(NULL);

	return NULL;
}

//---------------------------------------------------------
// Class K2hFtManage
//---------------------------------------------------------
K2hFtManage::K2hFtManage(void) : conf_lockval(FLCK_NOSHARED_MUTEX_VAL_UNLOCKED), wstack_lockval(FLCK_NOSHARED_MUTEX_VAL_UNLOCKED), exit_lockval(FLCK_NOSHARED_MUTEX_VAL_UNLOCKED), is_load_tp(false), is_init_perm(false), run_thread(false), base_mode(0), base_dmode(0), base_uid(0), base_gid(0)
{
}

K2hFtManage::~K2hFtManage(void)
{
	Clean();
}

bool K2hFtManage::Clean(void)
{
	// [NOTE]
	// When we caught many signal at exiting, this method deadlocks by conf_lockval.
	// Thus this method must be called only one.
	//
	if(!fullock::flck_trylock_noshared_mutex(&exit_lockval)){
		ERR_K2HFTPRN("Already in the end processing.");
		return false;
	}

	bool	result = true;
	// [NOTICE]
	// Not care for chmpx exiting...
	//

	// wait for thread exit
	if(run_thread){
		run_thread = false;

		void*	pretval = NULL;
		int		joinres;
		if(0 != (joinres = pthread_join(threadid, &pretval))){
			ERR_K2HFTPRN("Failed to wait timeup thread exiting by errno(%d), but continue,,,", joinres);
			result = false;
		}else{
			MSG_K2HFTPRN("Succeed to wait timeup thread exiting,");
		}
	}

	// Push and Free all write buffer
	if(!CloseAll()){
		ERR_K2HFTPRN("Failed to close all cache and stack buffer, but continue...");
		result = false;
	}

	// Stop plugins
	//
	// [NOTICE]
	// Must stop all plugins before K2hFtInfo is cleaned.
	// K2hFtPluginMan has all plugin objects which are pointers, the pointers
	// are freed in K2hFtInfo class.
	//
	if(!pluginman.StopPlugins(true)){
		ERR_K2HFTPRN("Failed to stop plugins, but continue...");
		result = false;
	}
	// remove temporary directory
	if(!K2hFtPluginMan::RemoveAllNamedPipeFiles()){
		ERR_K2HFTPRN("Failed to remove all named pipe files, but continue...");
		result = false;
	}

	// Lock
	while(!fullock::flck_trylock_noshared_mutex(&conf_lockval));

	// destroy fd cache
	if(!fdcache.Close()){
		ERR_K2HFTPRN("Failed to destroy fd cache object, but continue...");
		result = false;
	}

	// Unload transaction plugin
	if(is_load_tp && !K2HTransDynLib::get()->Unload()){
		ERR_K2HFTPRN("Failed to unload %s transaction plugin, but continue...", confinfo.GetCtpDtorPath());
		result = false;
	}

	// Detach k2hash
	if(k2hash.IsAttached() && !k2hash.Detach()){
		ERR_K2HFTPRN("Failed to detach k2hash for k2hftfuse, but continue...");
		result = false;
	}

	// clean configuration information
	confinfo.Clean();

	is_load_tp	= false;
	is_init_perm= false;
	base_mode	= 0;
	base_dmode	= 0;
	base_uid	= 0;
	base_gid	= 0;

	// UnLock
	fullock::flck_unlock_noshared_mutex(&conf_lockval);

	return result;
}

bool K2hFtManage::Initialize(const mode_t* pset_umask, const uid_t* pset_uid, const gid_t* pset_gid)
{
	// Lock
	while(!fullock::flck_trylock_noshared_mutex(&conf_lockval));

	if(confinfo.IsLoad()){
		ERR_K2HFTPRN("Already load configuration file, this initializing method must be called before loading it.");
		fullock::flck_unlock_noshared_mutex(&conf_lockval);		// Unlock
		return false;
	}

	mode_t	tmp_umask;
	if(!pset_umask){
		tmp_umask = umask(0);
		umask(tmp_umask);
	}else{
		tmp_umask = *pset_umask;
	}
	if(!pset_uid){
		base_uid = getuid();
	}else{
		base_uid = *pset_uid;
	}
	if(!pset_gid){
		base_gid = getgid();
	}else{
		base_gid = *pset_gid;
	}

	base_mode	= ((S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH) & ~tmp_umask) | S_IFREG;
	base_dmode	= ((S_IRWXU | S_IRWXG | S_IRWXO) & ~tmp_umask) | S_IFDIR | S_IXUSR;
	if(0 != (base_dmode & (S_IRGRP | S_IWGRP))){
		base_dmode |= S_IXGRP;
	}
	if(0 != (base_dmode & (S_IROTH | S_IWOTH))){
		base_dmode |= S_IXOTH;
	}

	is_init_perm = true;

	// UnLock
	fullock::flck_unlock_noshared_mutex(&conf_lockval);

	return true;
}

bool K2hFtManage::Initialize(const char* config, bool is_run_chmpx, const char* chmpxlog)
{
	// Lock
	while(!fullock::flck_trylock_noshared_mutex(&conf_lockval));

	// initialize fd cache object
	if(!fdcache.Initialize()){
		ERR_K2HFTPRN("Could not initialize fd cache object.");
		fullock::flck_unlock_noshared_mutex(&conf_lockval);		// Unlock
		return false;
	}

	// initialize plugin manager
	if(!pluginman.Initialize(&fdcache, K2hFtManage::GetMountPointString().empty() ? NULL : K2hFtManage::GetMountPoint())){
		ERR_K2HFTPRN("Could not initialize plugin manager.");
		fullock::flck_unlock_noshared_mutex(&conf_lockval);		// Unlock
		return false;
	}

	if(!is_init_perm){
		ERR_K2HFTPRN("Not initialized permission datas, Must initialize those before this methods.");
		fullock::flck_unlock_noshared_mutex(&conf_lockval);		// Unlock
		return false;
	}

	// load configuration file and build internal settings.
	if(!confinfo.Load(config, base_mode, base_dmode, base_uid, base_gid, pluginman)){
		ERR_K2HFTPRN("Something error occurred in loading configuration(%s).", config);
		fullock::flck_unlock_noshared_mutex(&conf_lockval);		// Unlock
		return false;
	}

	// For debugging
	if(IS_K2HFTDBG_DUMP()){
		confinfo.Dump();
	}

	// run timeup thread
	//
	// [NOTE]
	// If stocked lines is over zero or has timeup time, we need to run thread.
	//
	if(K2hFtWriteBuff::DEFAULT_LINE_MAX < K2hFtWriteBuff::GetStackLineMax() || K2hFtWriteBuff::DEFAULT_TIMEUP < K2hFtWriteBuff::GetStackTimeup()){
		run_thread = true;

		int	result;
		if(0 != (result = pthread_create(&threadid, NULL, K2hFtManage::TimeupWorkerProc, this))){
			ERR_K2HFTPRN("Failed to create timeup thread(return code = %d).", result);
			run_thread = false;
			return false;
		}
	}

	// run CHMPX
	if(is_run_chmpx){
		// [NOTICE]
		// We do not check sub process as chmpx running because of using system function.
		// And we run chmpx on background, so we can not check it's status(result).
		// Then we sleep 1 second here, if chmpx could not be run, k2hftfuse process would
		// not run with error.
		//
		string	strlogfile;
		if(K2HFT_ISEMPTYSTR(chmpxlog)){
			strlogfile = "/dev/null";		// default: /dev/null
		}else{
			strlogfile = chmpxlog;
		}
		string	chmpxcmd= string(K2HFT_CHMPX_COMMAND_PART1) + confinfo.Config + string(K2HFT_CHMPX_COMMAND_PART2) + strlogfile + string(K2HFT_CHMPX_COMMAND_PART3);
		int		result	= system(chmpxcmd.c_str());

		MSG_K2HFTPRN("run chmpx slave process with conf file(%s), the result is %d. sleep 1 second.", confinfo.Config.c_str(), result);
		sleep(1);
	}

	// set transaction thread count
	if(!K2HShm::SetTransThreadPool(confinfo.DtorThreadCnt)){
		ERR_K2HFTPRN("Failed to set transaction thread pool(count=%d).", confinfo.DtorThreadCnt);
		fullock::flck_unlock_noshared_mutex(&conf_lockval);		// Unlock
		confinfo.Clean();										// clean
		return false;
	}

	// initialize k2hash file(memory)
	bool	bresult = false;
	if(confinfo.IsMemType){
		// memory type
		bresult = k2hash.AttachMem(confinfo.MaskBitCnt, confinfo.CMaskBitCnt, confinfo.MaxElementCnt, confinfo.PageSize);

	}else if(confinfo.K2hFilePath.empty()){
		// temporary type
		string	temppath = string(K2HFT_TEMPK2H_PATH_PREFIX) + to_string(getpid()) + string(K2HFT_TEMPK2H_PATH_SUFFIX);

		// if temporary file exists, remove and initialize it.
		if(!remove_exist_file(temppath.c_str())){
			ERR_K2HFTPRN("Found temporary file(%s), but could not initialize it.", temppath.c_str());
			bresult = false;
		}else{
			bresult = k2hash.Attach(temppath.c_str(), false, true, true, confinfo.IsFullmap, confinfo.MaskBitCnt, confinfo.CMaskBitCnt, confinfo.MaxElementCnt, confinfo.PageSize);
		}

	}else{
		// file type
		if(confinfo.IsInitialize){
			// if file exists, remove it.
			if(!remove_exist_file(confinfo.K2hFilePath.c_str())){
				ERR_K2HFTPRN("Found file(%s), but could not initialize it.", confinfo.K2hFilePath.c_str());
				bresult = false;
			}else{
				bresult = k2hash.Attach(confinfo.K2hFilePath.c_str(), false, true, false, confinfo.IsFullmap, confinfo.MaskBitCnt, confinfo.CMaskBitCnt, confinfo.MaxElementCnt, confinfo.PageSize);
			}
		}else{
			bresult = k2hash.Attach(confinfo.K2hFilePath.c_str(), false, true, false, confinfo.IsFullmap, confinfo.MaskBitCnt, confinfo.CMaskBitCnt, confinfo.MaxElementCnt, confinfo.PageSize);
		}
	}
	if(!bresult){
		ERR_K2HFTPRN("Could not build internal k2hash file(memory) for k2hftfuse.");
		fullock::flck_unlock_noshared_mutex(&conf_lockval);		// Unlock
		confinfo.Clean();										// clean
		return false;
	}

	// load k2htpdtor transaction plugin
	if(!is_load_tp && !K2HTransDynLib::get()->Load(confinfo.GetCtpDtorPath())){
		ERR_K2HFTPRN("Could not load %s transaction plugin.", confinfo.GetCtpDtorPath());
		fullock::flck_unlock_noshared_mutex(&conf_lockval);		// Unlock
		k2hash.Detach();
		confinfo.Clean();										// clean
		return false;
	}

	// set internal variable
	is_load_tp = true;

	// enable transaction plugin
	if(!k2hash.EnableTransaction(NULL, NULL, 0L, reinterpret_cast<const unsigned char*>(confinfo.Config.c_str()), confinfo.Config.length() + 1), confinfo.GetExpireTime()){
		ERR_K2HFTPRN("Could not enable %s transaction plugin.", confinfo.GetCtpDtorPath());
		ERR_K2HFTPRN("Please check chmpx slave process running, if it does not run, k2hftfuse can not run.");

		fullock::flck_unlock_noshared_mutex(&conf_lockval);		// Unlock
		K2HTransDynLib::get()->Unload();
		is_load_tp = false;
		k2hash.Detach();
		confinfo.Clean();		// clean
		return false;
	}

	// Run all plugin(but not execute process)
	if(!pluginman.RunPlugins(true)){
		ERR_K2HFTPRN("Could not run plugin child process.");

		fullock::flck_unlock_noshared_mutex(&conf_lockval);		// Unlock
		K2HTransDynLib::get()->Unload();
		is_load_tp = false;
		k2hash.Detach();
		confinfo.Clean();		// clean
		return false;
	}

	// UnLock
	fullock::flck_unlock_noshared_mutex(&conf_lockval);

	return true;
}

bool K2hFtManage::Push(const unsigned char* data, size_t length, uint64_t filehandle, pid_t pid)
{
	while(!fullock::flck_trylock_noshared_mutex(&wstack_lockval));		// LOCK

	K2hFtWriteBuff*				pwbuf	= NULL;
	k2hftwbufmap_t::iterator	iter	= wbstackmap.find(filehandle);
	if(iter == wbstackmap.end() || NULL == (pwbuf = iter->second)){
		// need to make new
		pwbuf = new K2hFtWriteBuff(this, filehandle);
		wbstackmap[filehandle] = pwbuf;
	}
	fullock::flck_unlock_noshared_mutex(&wstack_lockval);				// UNLOCK

	// Push
	if(!pwbuf->Push(data, length, IsBinMode(), pid)){
		ERR_K2HFTPRN("Failed to push data(%p) length(%zu) for pid(%d), fh(0x%" PRIx64 ").", data, length, pid, filehandle);
		return false;
	}
	// Check & write data to k2hash
	if(!CheckPush(pwbuf, filehandle)){
		ERR_K2HFTPRN("Something error occurred during pop and writing. but return true from this method.");
	}
	return true;
}

// [NOTE]
// Do work only binary mode. And do same as Close()
//
bool K2hFtManage::Flush(uint64_t filehandle, pid_t pid)
{
	if(!IsBinMode()){
		WAN_K2HFTPRN("Now not binary mode, why does call this method?");
		return true;
	}
	return Close(filehandle, pid);
}

// [NOTE]
// If there is no cache data after pushing data for filehandle, this method will delete
// map from mapping list.
//
bool K2hFtManage::Close(uint64_t filehandle, pid_t pid)
{
	while(!fullock::flck_trylock_noshared_mutex(&wstack_lockval));		// LOCK

	K2hFtWriteBuff*				pwbuf	= NULL;
	k2hftwbufmap_t::iterator	iter	= wbstackmap.find(filehandle);
	if(iter == wbstackmap.end() || NULL == (pwbuf = iter->second)){
		// nothing to do
		if(iter != wbstackmap.end()){
			wbstackmap.erase(iter);
		}
		fullock::flck_unlock_noshared_mutex(&wstack_lockval);			// UNLOCK
		return true;
	}
	fullock::flck_unlock_noshared_mutex(&wstack_lockval);				// UNLOCK

	// Force Push
	if(!pwbuf->ForcePush(IsBinMode(), pid)){
		ERR_K2HFTPRN("Failed to force push for pid(%d), fh(0x%" PRIx64 ").", pid, filehandle);
		return false;
	}

	// Check & write data to k2hash
	if(!CheckPush(pwbuf, filehandle, true)){
		ERR_K2HFTPRN("Something error occurred during pop and writing. but return true from this method.");
	}
	return true;
}

// [NOTE]
// Force push all stack data, because this method is called when shutdown.
//
bool K2hFtManage::CloseAll(void)
{
	while(!fullock::flck_trylock_noshared_mutex(&wstack_lockval));		// LOCK

	for(k2hftwbufmap_t::iterator iter = wbstackmap.begin(); iter != wbstackmap.end(); wbstackmap.erase(iter++)){
		uint64_t		filehandle	= iter->first;
		K2hFtWriteBuff*	pwbuf		= iter->second;

		if(pwbuf){
			// Force Push All
			if(!pwbuf->ForceAllPush(IsBinMode())){
				ERR_K2HFTPRN("Failed to force push for all, file handle(0x%" PRIx64 "), but continue...", filehandle);
			}
			// Check & write data to k2hash
			if(!CheckPush(pwbuf, filehandle, true)){
				ERR_K2HFTPRN("Something error occurred during pop and writing for file handle(0x%" PRIx64 "). but continue...", filehandle);
			}
			K2HFT_DELETE(pwbuf);
		}
	}
	free_k2hftwbufmap(wbstackmap);

	fullock::flck_unlock_noshared_mutex(&wstack_lockval);				// UNLOCK

	return true;
}

// [NOTE]
// If returned pointer is not same data pointer, MUST free returned pointer.
// It is allocated in K2hFtInfo::CvtPushData().
//
unsigned char* K2hFtManage::CvtPushData(const unsigned char* data, size_t length, uint64_t filehandle, size_t& cvtlength) const
{
	return confinfo.CvtPushData(data, length, filehandle, cvtlength);
}

bool K2hFtManage::CheckPush(K2hFtWriteBuff* pwbuf, uint64_t filehandle, bool is_force)
{
	if(!is_force && !pwbuf->IsStackLimit()){
		//MSG_K2HFTPRN("Not limit for stacked data yet.");
		return true;
	}

	// get stacked data formatted by k2hash value.
	PK2HFTVALUE	pValue = pwbuf->Pop();
	if(!pValue){
		//MSG_K2HFTPRN("Could not make k2hash value data.");
		return true;
	}

	// Do processing...
	bool	result;
	if(false == (result = confinfo.Processing(&k2hash, pValue, filehandle, fdcache, pluginman))){
		ERR_K2HFTPRN("Something error occurred during processing, skip it.");
	}
	K2HFT_FREE(pValue);

	return result;
}

/*
 * VIM modelines
 *
 * vim:set ts=4 fenc=utf-8:
 */
