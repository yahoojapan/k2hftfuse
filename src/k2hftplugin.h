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
 * CREATE:   Tue Oct 20 2015
 * REVISION:
 *
 */

#ifndef	K2HFTPLUGIN_H
#define K2HFTPLUGIN_H

#include <fullock/flckstructure.h>
#include <fullock/flckbaselist.tcc>

#include <string>
#include <vector>
#include <map>

#include "k2hftfdcache.h"
#include "k2hftutil.h"

//---------------------------------------------------------
// Structure
//---------------------------------------------------------
typedef struct k2hft_plugin_info{
	std::string		BaseParam;
	std::string		OutputPath;
	std::string		PipeFilePath;			// named pipe file path for plugin input
	dev_t			st_dev;					// device id for output file
	ino_t			st_ino;					// inode id for output file
	mode_t			mode;					// output file mode
	volatile pid_t	pid;
	int				pipe_input;				// named pipe(fifo)
	int				last_status;
	int				exit_count;
	bool			not_execute;			// true when plugin in rule is directory type.
	bool			auto_restart;			// K2hFtPluginMan internal use
	volatile int	lockval;				// synchronized writing

	k2hft_plugin_info(void) : BaseParam(""), OutputPath(""), PipeFilePath(""), st_dev(0), st_ino(0), mode(0), pid(K2HFT_INVALID_PID), pipe_input(K2HFT_INVALID_HANDLE), last_status(0), exit_count(0), not_execute(false), auto_restart(true), lockval(FLCK_NOSHARED_MUTEX_VAL_UNLOCKED) {}

}K2HFT_PLUGIN, *PK2HFT_PLUGIN;

typedef std::map<volatile pid_t, PK2HFT_PLUGIN>	k2hftpluginmap_t;
typedef std::vector<PK2HFT_PLUGIN>				k2hftpluginlist_t;

//---------------------------------------------------------
// Class K2hFtPluginMan
//---------------------------------------------------------
class K2hFtPluginMan
{
	protected:
		static const int		WRITE_RETRY_MAX = 500;	// retry count(500 * 100us = 50ms)
		static volatile bool	thread_exit;			// request flag for watching children thread exit
		static volatile bool	is_run_thread;
		static pthread_t		watch_thread_tid;		// watch children thread id

		K2hFtFdCache*			pfdcache;				// fd cache class
		std::string				mountpoint;				// backup for mount point path
		std::string				condname;
		std::string				mutexname;
		volatile int			lockval;				// the value to run_plugin and stop_plugin
		k2hftpluginmap_t		run_plugin;
		k2hftpluginlist_t		stop_plugin;

	protected:
		static bool BlockSignal(int sig);
		static bool AddFdFlags(int fd, int flags);			// move to util
		static bool ClearFdFlags(int fd, int flags);		// move to util
		static void* WatchChildrenExit(void* param);
		static bool CleanupChildExit(K2hFtPluginMan* pMan, pid_t childpid, int exitstatus, bool do_restart);
		static bool ParseExecParam(const char* base, std::string& filename, strlst_t& argvs, strlst_t& envs);
		static pid_t RunPluginProcess(const char* pluginparam, const char* outputfile, const char* pipepath, int& pipe_input, const char* condname, const char* mutexname);

		bool RunWatchThread(void);
		bool StopWatchThread(void);
		bool RunPlugin(PK2HFT_PLUGIN pplugin, bool is_wait_cond);
		bool KillPlugin(PK2HFT_PLUGIN pplugin);
		bool StopPlugin(PK2HFT_PLUGIN pplugin);

	public:
		static bool BuildNamedPipeFile(std::string& path);
		static bool RemoveAllNamedPipeFiles(void);

		K2hFtPluginMan(void);
		virtual ~K2hFtPluginMan(void);

		bool Clean(void);
		bool Initialize(K2hFtFdCache* pfd, const char* pmountpoint = NULL);
		bool IsEmpty(void) const { return (run_plugin.empty() && stop_plugin.empty()); }

		bool AddPluginInfo(PK2HFT_PLUGIN pplugin);
		bool RunAddPlugin(PK2HFT_PLUGIN pplugin);
		bool RunPlugins(bool is_wait_cond = false);
		bool ExecPlugins(void);
		bool StopPlugins(bool force = false);

		bool Write(PK2HFT_PLUGIN pplugin, unsigned char* pdata, size_t length);
};

#endif	// K2HFTPLUGIN_H

/*
 * VIM modelines
 *
 * vim:set ts=4 fenc=utf-8:
 */
