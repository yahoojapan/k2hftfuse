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

#ifndef	K2HFTMAN_H
#define K2HFTMAN_H

#include <pthread.h>
#include <k2hash/k2hshm.h>

#include "k2hftinfo.h"
#include "k2hftwbuf.h"
#include "k2hftfdcache.h"

//---------------------------------------------------------
// Class K2hFtManage
//---------------------------------------------------------
// [NOTE]
// wbstackmap mapping will grow only, does not reduce.
// Thus we only lock wbstackmap for seaching by file handle,
// and we do not have to lock it after searching.
//
class K2hFtManage
{
	protected:
		volatile int		conf_lockval;			// like mutex for valiables
		volatile int		wstack_lockval;			// like mutex for valiables
		volatile int		exit_lockval;			// like mutex for valiables
		bool				is_load_tp;				// transaction plugin(dtor) load flag
		bool				is_init_perm;			// initialize base_*** permission flag

		volatile bool		run_thread;				// watch timeup thread flag
		pthread_t			threadid;				// watch timeup thread

		mode_t				base_mode;				// default mode for files
		mode_t				base_dmode;				// default mode for directories
		uid_t				base_uid;				// default uid
		gid_t				base_gid;				// default gid

		K2hFtInfo			confinfo;				// configuration file class object
		K2hFtPluginMan		pluginman;				// plugin manager
		K2HShm				k2hash;					// k2hash
		k2hftwbufmap_t		wbstackmap;				// stack for write buffer
		K2hFtFdCache		fdcache;				// fd cache class

	protected:
		static void* TimeupWorkerProc(void* param);

		bool CheckPush(K2hFtWriteBuff* pwbuf, uint64_t filehandle, bool is_force = false);

	public:
		static std::string& GetMountPointString(void);	// mountpoint
		static const char* GetMountPoint(void);
		static bool SetMountPoint(const char* path);

		K2hFtManage(void);
		virtual ~K2hFtManage(void);

		bool Clean(void);
		bool Initialize(mode_t* pset_umask, uid_t* pset_uid, gid_t* pset_gid);
		bool Initialize(const char* config, bool is_run_chmpx = false, const char* chmpxlog = NULL);

		bool IsBinMode(void) const { return confinfo.IsBinMode(); }
		bool FindPath(const char* path, struct stat& stbuf) const { return confinfo.FindPath(path, stbuf); }
		uint64_t GetFileHandle(const char* path) const { return confinfo.GetFileHandle(path); }

		bool ReadDir(const char* path, k2hftldlist_t& list) const { return confinfo.ReadDir(path, list); }
		bool TruncateZero(const char* path) { return confinfo.TruncateZero(path); }
		bool SetOwner(const char* path, uid_t uid, gid_t gid) { return confinfo.SetOwner(path, uid, gid); }
		bool SetMode(const char* path, mode_t mode) { return confinfo.SetMode(path, mode); }
		bool SetTimespec(const char* path, const struct timespec& ts) { return confinfo.SetTimespec(path, ts); }

		// [NOTE]
		// K2hFtInfo is only growing up (because it is not so many files.)
		// Thus we do not lock it, and get good performance.
		bool CreateFile(const char* path, mode_t mode, uid_t uid, gid_t gid, uint64_t& handle) { return confinfo.CreateFile(path, mode, uid, gid, handle, pluginman); }
		bool UpdateMTime(uint64_t handle) { return confinfo.UpdateMTime(handle); }

		bool Push(const unsigned char* data, size_t length, uint64_t filehandle, pid_t pid);
		bool Flush(uint64_t filehandle, pid_t pid);
		bool Close(uint64_t filehandle, pid_t pid);
		bool CloseAll(void);

		bool ExecPlugins(void) { return pluginman.ExecPlugins(); }

		unsigned char* CvtPushData(const unsigned char* data, size_t length, uint64_t filehandle, size_t& cvtlength) const;
};

#endif	// K2HFTMAN_H

/*
 * VIM modelines
 *
 * vim:set ts=4 fenc=utf-8:
 */
