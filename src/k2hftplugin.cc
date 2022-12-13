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

#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/signalfd.h>
#include <fullock/flckshm.h>
#include <fullock/flckutil.h>

#include "k2hftcommon.h"
#include "k2hftplugin.h"
#include "k2hftutil.h"
#include "k2hftdbg.h"

using namespace std;

//---------------------------------------------------------
// Symbols
//---------------------------------------------------------
#define	K2HFTFUSE_NPIPE_DIR_PREFIX			"/tmp/k2hftfuse_np_"
#define	K2HFTFUSE_NPIPE_FILE_PREFIX			"/np_"
#define	K2HFTFUSE_RMDIR_CMD					"rm -rf "

#define	K2HFT_PLUGIN_CONDNAME_PREFIX		"K2HFTPLGIN_COND_"
#define	K2HFT_PLUGIN_MUTEXNAME_PREFIX		"K2HFTPLGIN_MUTEX_"

#define	CHILD_READ_FD_PIPE_POS				0
#define	PARENT_WRITE_FD_PIPE_POS			(CHILD_READ_FD_PIPE_POS + 1)
#define	PIPE_FD_COUNT						(PARENT_WRITE_FD_PIPE_POS + 1)

#define	MAX_WAIT_COUNT_UNTIL_KILL			500			// 500 * 1ms = 500ms

//---------------------------------------------------------
// Class variables
//---------------------------------------------------------
const int		K2hFtPluginMan::WRITE_RETRY_MAX;
volatile bool	K2hFtPluginMan::thread_exit			= true;
volatile bool	K2hFtPluginMan::is_run_thread		= false;
pthread_t		K2hFtPluginMan::watch_thread_tid;

//---------------------------------------------------------
// Class methods
//---------------------------------------------------------
bool K2hFtPluginMan::BlockSignal(int sig)
{
	sigset_t	blockmask;
	sigemptyset(&blockmask);
	sigaddset(&blockmask, sig);
	if(-1 == sigprocmask(SIG_BLOCK, &blockmask, NULL)){
		ERR_K2HFTPRN("Could not block signal(%d) by errno(%d)", sig, errno);
		return false;
	}
	return true;
}

bool K2hFtPluginMan::AddFdFlags(int fd, int flags)
{
	int	value;
	if(-1 == (value = fcntl(fd, F_GETFL, 0))){
		ERR_K2HFTPRN("Could not get file open flags by errno(%d)", errno);
		return false;
	}
	value |= flags;
	if(-1 == (value = fcntl(fd, F_SETFL, value))){
		ERR_K2HFTPRN("Could not set file open flags(add %d, result %d) by errno(%d)", flags, value, errno);
		return false;
	}
	return true;
}

bool K2hFtPluginMan::ClearFdFlags(int fd, int flags)
{
	int	value;
	if(-1 == (value = fcntl(fd, F_GETFL, 0))){
		ERR_K2HFTPRN("Could not get file open flags by errno(%d)", errno);
		return false;
	}
	value &= ~flags;
	if(-1 == (value = fcntl(fd, F_SETFL, value))){
		ERR_K2HFTPRN("Could not clear file open flags(clear %d, result %d) by errno(%d)", flags, value, errno);
		return false;
	}
	return true;
}

void* K2hFtPluginMan::WatchChildrenExit(void* param)
{
	if(!param){
		ERR_K2HFTPRN("param is NULL.");
		pthread_exit(NULL);
	}
	K2hFtPluginMan*	pMan	= static_cast<K2hFtPluginMan*>(param);

	// block SIGCHLD
	if(!K2hFtPluginMan::BlockSignal(SIGCHLD)){
		ERR_K2HFTPRN("Could not block SIGCHLD.");
		pthread_exit(NULL);
	}

	// loop
	while(!K2hFtPluginMan::thread_exit){
		//---------------------------
		// check children
		//---------------------------
		do{
			// waitpid
			pid_t	childpid;
			int		status	= 0;
			if(0 < (childpid= waitpid(-1, &status, WNOHANG | __WALL))){
				MSG_K2HFTPRN("Succeed waitpid for pid(%d)", childpid);
			}else if(0 == childpid){
				//MSG_K2HFTPRN("No status changed children.");
				break;
			}else{
				if(ECHILD != errno){
					WAN_K2HFTPRN("Failed to waitpid by errno(%d)", errno);
					break;
				}
				// [NOTE]
				// we set SIG_IGN to SIGCHILD, thus we caught this error after all children exited.
				// this case is no problem.
			}

			//---------------------------
			// cleanup & restart
			//---------------------------
			if(0 < childpid && !K2hFtPluginMan::CleanupChildExit(pMan, childpid, WEXITSTATUS(status), !K2hFtPluginMan::thread_exit)){
				ERR_K2HFTPRN("Something error occurred after plugin(%d) exited for waitpid/stop/restart. but continue...", childpid);
			}
		}while(!K2hFtPluginMan::thread_exit);

		// sleep
		struct timespec	waittime = {0, 50 * 1000 * 1000};			// 50ms
		nanosleep(&waittime, NULL);
	}
	MSG_K2HFTPRN("Exit watch thread.");

	pthread_exit(NULL);
	return NULL;
}

bool K2hFtPluginMan::CleanupChildExit(K2hFtPluginMan* pMan, pid_t childpid, int exitstatus, bool do_restart)
{
	if(!pMan){
		ERR_K2HFTPRN("pMan is NULL.");
		return false;
	}

	// check exiting child
	while(!fullock::flck_trylock_noshared_mutex(&(pMan->lockval)));	// MUTEX LOCK

	// pid -> plugin
	k2hftpluginmap_t::iterator	iter;
	if(pMan->run_plugin.end() == (iter = pMan->run_plugin.find(childpid))){
		WAN_K2HFTPRN("could not find pid(%d) process in running children map.", childpid);
		fullock::flck_unlock_noshared_mutex(&(pMan->lockval));		// MUTEX UNLOCK
		return false;
	}

	// remove plugin from running children map
	PK2HFT_PLUGIN	pplugin = iter->second;
	if(!pplugin){
		ERR_K2HFTPRN("pid(%d) process in running children map is NULL PLUGIN, so remove it.", childpid);
		pMan->run_plugin.erase(iter);
		fullock::flck_unlock_noshared_mutex(&(pMan->lockval));		// MUTEX UNLOCK
		return false;
	}
	pMan->run_plugin.erase(iter);

	// cleanup pplugin data
	pplugin->pid		= K2HFT_INVALID_PID;
	pplugin->last_status= exitstatus;
	pplugin->exit_count++;
	// [NOTICE]
	// not close plugin's named pipe handle here.

	// add plugin from stopping children map
	pMan->stop_plugin.push_back(pplugin);

	fullock::flck_unlock_noshared_mutex(&(pMan->lockval));			// MUTEX UNLOCK

	MSG_K2HFTPRN("child plugin([%s] %s) is removed from running list and added stop list.", pplugin->OutputPath.empty() ? "empty" : pplugin->OutputPath.c_str(), pplugin->BaseParam.c_str());

	// run all stopping plugins(run as soon as possible)
	if(do_restart){
		if(!pMan->RunPlugins()){
			ERR_K2HFTPRN("Something error occurred during restarting plugins");
			return false;
		}
	}
	return true;
}

bool K2hFtPluginMan::ParseExecParam(const char* base, string& filename, strlst_t& argvs, strlst_t& envs)
{
	if(K2HFT_ISEMPTYSTR(base)){
		ERR_K2HFTPRN("base parameter is empty.");
		return false;
	}
	// convert
	strlst_t	baselist;
	if(0 == convert_string_to_strlst(base, baselist)){
		ERR_K2HFTPRN("There is no string token in base parameter.");
		return false;
	}

	// parse string list to filename/argvs/envs
	argvs.clear();
	envs.clear();
	for(strlst_t::const_iterator iter = baselist.begin(); iter != baselist.end(); ++iter){
		if(string::npos != (*iter).find('=')){
			// env parameter
			envs.push_back(*iter);
		}else{
			// filename
			filename = (*iter);

			// all string list after filename are argv.
			argvs.push_back(filename);		// 1'st argv is filename
			for(++iter; iter != baselist.end(); ++iter){
				argvs.push_back(*iter);
			}
			break;
		}
	}
	return (!filename.empty());
}

//
// Make uniq named pipe file.
// 
bool K2hFtPluginMan::BuildNamedPipeFile(string& path)
{
	static bool		init_time = false;
	static time_t	base_number;

	if(!init_time){
		base_number	= time(NULL);
		init_time	= true;
	}

	string	abspath;								// tmp
	string	nppath(K2HFTFUSE_NPIPE_DIR_PREFIX);
	nppath += to_string(getpid());

	// check & make dir
	if(!mkdir_by_abs_path(nppath.c_str(), abspath)){
		ERR_K2HFTPRN("could not make directory(%s) for named pipe files.", nppath.c_str());
		return false;
	}

	// make file path
	nppath += K2HFTFUSE_NPIPE_FILE_PREFIX;
	nppath += to_string(base_number);
	base_number++;

	// make named pipe
	if(-1 == mkfifo(nppath.c_str(), S_IRUSR | S_IWUSR)){
		ERR_K2HFTPRN("could not make named pipe(%s) by errno(%d).", nppath.c_str(), errno);
		return false;
	}
	path = nppath;

	return true;
}

bool K2hFtPluginMan::RemoveAllNamedPipeFiles(void)
{
	string	rmcmd(K2HFTFUSE_RMDIR_CMD);
	rmcmd	+= K2HFTFUSE_NPIPE_DIR_PREFIX;
	rmcmd	+= to_string(getpid());

	// no check error
	int	rtncode;
	if(0 != (rtncode = system(rmcmd.c_str()))){
		ERR_K2HFTPRN("Could not remove named pipes directory, return code(%d)", rtncode);
		return false;
	}
	return true;
}

pid_t K2hFtPluginMan::RunPluginProcess(const char* pluginparam, const char* outputfile, const char* pipepath, int& pipe_input, const char* condname, const char* mutexname)
{
	if(K2HFT_ISEMPTYSTR(pluginparam) || K2HFT_ISEMPTYSTR(pipepath)){
		ERR_K2HFTPRN("parameters are empty.");
		return K2HFT_INVALID_PID;
	}
	string	ofile = (K2HFT_ISEMPTYSTR(outputfile) ? "/dev/null" : outputfile);

	// fork
	pid_t	childpid = fork();
	if(-1 == childpid){
		// error
		ERR_K2HFTPRN("Could not fork for \"%s\" plugin by errno(%d).", pluginparam, errno);
		return K2HFT_INVALID_PID;

	}else if(0 == childpid){
		// child process

		// for open named PIPE without blocking
		int	child_input_pipe;
		{
			// open READ mode with nonblocking
			if(-1 == (child_input_pipe = open(pipepath, O_RDONLY | O_NONBLOCK))){
				ERR_K2HFTPRN("Could not open read(non blocking) only named pipe(%s) for \"%s\" plugin by errno(%d).", pipepath, pluginparam, errno);
				exit(EXIT_FAILURE);
			}
			// open WRITE with blocking(never use and close this)
			int	child_wr_pipe;
			if(-1 == (child_wr_pipe = open(pipepath, O_WRONLY))){
				ERR_K2HFTPRN("Could not open write only named pipe(%s) for \"%s\" plugin by errno(%d).", pipepath, pluginparam, errno);
				K2HFT_CLOSE(child_input_pipe);
				exit(EXIT_FAILURE);
			}
			// set blocking mode to READ pipe
			if(!ClearFdFlags(child_input_pipe, O_NONBLOCK)){
				ERR_K2HFTPRN("Could not unset blocking to readable named pipe(%s) for \"%s\" plugin.", pipepath, pluginparam);
				K2HFT_CLOSE(child_input_pipe);
				K2HFT_CLOSE(child_wr_pipe);
				exit(EXIT_FAILURE);
			}
			// close writer fd
			// cppcheck-suppress unreadVariable
			K2HFT_CLOSE(child_wr_pipe);
		}

		// make process parameters
		string	filename;
		char**	argvs;
		char**	envs;
		{
			strlst_t	argvlist;
			strlst_t	envlist;
			if(!K2hFtPluginMan::ParseExecParam(pluginparam, filename, argvlist, envlist)){
				ERR_K2HFTPRN("could not parse base parameter for \"%s\" plugin.", pluginparam);
				K2HFT_CLOSE(child_input_pipe);
				exit(EXIT_FAILURE);
			}
			if(NULL == (argvs = convert_strlst_to_chararray(argvlist)) || NULL == (envs = convert_strlst_to_chararray(envlist))){
				ERR_K2HFTPRN("could not parse base parameter for \"%s\" plugin.", pluginparam);
				free_chararray(argvs);
				free_chararray(envs);
				K2HFT_CLOSE(child_input_pipe);
				exit(EXIT_FAILURE);
			}
		}

		// signal
		{
			// unblock
			sigset_t	blockmask;
			sigemptyset(&blockmask);
			sigaddset(&blockmask, SIGPIPE);
			sigaddset(&blockmask, SIGCHLD);
			if(-1 == sigprocmask(SIG_UNBLOCK, &blockmask, NULL)){
				ERR_K2HFTPRN("Could not block SIGPIPE by errno(%d)", errno);
				exit(EXIT_FAILURE);
			}

			// set default handlers
			struct sigaction	saDefault;
			sigemptyset(&saDefault.sa_mask);
			saDefault.sa_handler= SIG_DFL;
			saDefault.sa_flags	= 0;
			if(	-1 == sigaction(SIGINT, &saDefault, NULL)	||
				-1 == sigaction(SIGQUIT, &saDefault, NULL)	||
				-1 == sigaction(SIGCHLD, &saDefault, NULL)	||
				-1 == sigaction(SIGPIPE, &saDefault, NULL)	)
			{
				ERR_K2HFTPRN("Could not set default signal handler(INT/QUIT/CHLD/PIPE) for \"%s\" plugin by errno(%d), but continue...", pluginparam, errno);
			}
		}

		// wait for running k2hftfuse
		//
		// [NOTE]
		// The condition lock object is signaled from main FUSE process when calling initialize-handler.
		//
		if(!K2HFT_ISEMPTYSTR(condname) && !K2HFT_ISEMPTYSTR(mutexname)){
			FlShm	flobj;
			int		result;
			if(0 != (result = flobj.Lock(mutexname))){
				ERR_K2HFTPRN("Failed to lock named mutex(%s) for \"%s\" plugin by errno(%d).", mutexname, pluginparam, result);
				free_chararray(argvs);
				free_chararray(envs);
				K2HFT_CLOSE(child_input_pipe);
				exit(EXIT_FAILURE);
			}
			if(0 != (result = flobj.Wait(condname, mutexname))){
				ERR_K2HFTPRN("Failed to wait condition(%s) for \"%s\" plugin by errno(%d).", condname, pluginparam, result);
				free_chararray(argvs);
				free_chararray(envs);
				K2HFT_CLOSE(child_input_pipe);
				exit(EXIT_FAILURE);
			}
			if(0 != (result = flobj.Unlock(mutexname))){
				ERR_K2HFTPRN("Failed to unlock named mutex(%s) for \"%s\" plugin by errno(%d).", mutexname, pluginparam, result);
				free_chararray(argvs);
				free_chararray(envs);
				K2HFT_CLOSE(child_input_pipe);
				exit(EXIT_FAILURE);
			}
		}

		// open output file
		int	noatime_flag = is_file_character_device(ofile.c_str()) ? 0 : O_NOATIME;		// must not set noatime for character device
		int	fd;
		if(-1 == (fd = open(ofile.c_str(), O_WRONLY | O_CREAT | O_APPEND | noatime_flag, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH))){
			//
			// [NOTICE]
			// If output file is on FUSE mount disk and it does not finish initializing FUSE before opening it,
			// we need to wait finishing FUSE.
			// Then try to open after waiting little.
			//
			WAN_K2HFTPRN("could not open output file(%s) for \"%s\" plugin by errno(%d), thus try to wait a while and retry to open...", ofile.c_str(), pluginparam, errno);

			struct timespec	sleeptime = {0L, 300 * 1000 * 1000};	// 300ms
			nanosleep(&sleeptime, NULL);

			if(-1 == (fd = open(ofile.c_str(), O_WRONLY | O_CREAT | O_APPEND | noatime_flag, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH))){
				ERR_K2HFTPRN("could not open output file(%s) for \"%s\" plugin by errno(%d).", ofile.c_str(), pluginparam, errno);
				free_chararray(argvs);
				free_chararray(envs);
				K2HFT_CLOSE(child_input_pipe);
				exit(EXIT_FAILURE);
			}
		}

		// duplicate pipe/fd to stdin/stdout
		if(-1 == dup2(child_input_pipe, STDIN_FILENO) || -1 == dup2(fd, STDOUT_FILENO)){
			ERR_K2HFTPRN("could not duplicate stdin/stdout for \"%s\" plugin by errno(%d).", pluginparam, errno);
			free_chararray(argvs);
			free_chararray(envs);
			K2HFT_CLOSE(child_input_pipe);
			K2HFT_CLOSE(fd);
			exit(EXIT_FAILURE);
		}
		// close all
		// cppcheck-suppress unreadVariable
		K2HFT_CLOSE(child_input_pipe);
		// cppcheck-suppress unreadVariable
		K2HFT_CLOSE(fd);

		MSG_K2HFTPRN("CHILD PROCESS: Started child plugin(%s) process(pid=%d).", filename.c_str(), getpid());

		// execute
		if(-1 == execvpe(filename.c_str(), argvs, envs)){
			ERR_K2HFTPRN("could not execute \"%s\" plugin by errno(%d).", pluginparam, errno);
			free_chararray(argvs);
			free_chararray(envs);
			exit(EXIT_FAILURE);
		}

	}else{
		// parent process
		if(K2HFT_INVALID_HANDLE == pipe_input){
			// open input pipe
			if(K2HFT_INVALID_HANDLE == (pipe_input = open(pipepath, O_WRONLY | O_CLOEXEC))){
				ERR_K2HFTPRN("Could not open write only named pipe(%s) for \"%s\" plugin by errno(%d).", pipepath, pluginparam, errno);
				return K2HFT_INVALID_PID;
			}
		}
	}
	return childpid;
}

//---------------------------------------------------------
// Methods
//---------------------------------------------------------
K2hFtPluginMan::K2hFtPluginMan(void) : pfdcache(NULL), mountpoint(""), condname(K2HFT_PLUGIN_CONDNAME_PREFIX), mutexname(K2HFT_PLUGIN_MUTEXNAME_PREFIX), lockval(FLCK_NOSHARED_MUTEX_VAL_UNLOCKED)
{
	// set named condition/mutex value
	condname	+= to_string(getpid());
	mutexname	+= to_string(getpid());
}

K2hFtPluginMan::~K2hFtPluginMan(void)
{
	Clean();
}

bool K2hFtPluginMan::Clean(void)
{
	bool	result = true;
	if(!StopPlugins(true)){
		ERR_K2HFTPRN("Something error occurred during stopping plugins, but continue...");
		result = false;
	}

	if(!StopWatchThread()){
		ERR_K2HFTPRN("Something error occurred during stopping plugins, but continue...");
		result = false;
	}

	// remove temporary directory
	if(!K2hFtPluginMan::RemoveAllNamedPipeFiles()){
		ERR_K2HFTPRN("Failed to remove all named pipe files, but continue...");
		result = false;
	}
	return result;
}

bool K2hFtPluginMan::Initialize(K2hFtFdCache* pfd, const char* pmountpoint)
{
	if(!pfd){
		ERR_K2HFTPRN("parameter is wrong.");
		return false;
	}
	pfdcache = pfd;

	if(!K2HFT_ISEMPTYSTR(pmountpoint)){
		mountpoint = pmountpoint;
	}

	// block SIGPIPE
	if(!K2hFtPluginMan::BlockSignal(SIGPIPE)){
		ERR_K2HFTPRN("Could not block SIGPIPE.");
		return false;
	}

	// block SIGCHLD
	if(!K2hFtPluginMan::BlockSignal(SIGCHLD)){
		ERR_K2HFTPRN("Could not block SIGCHLD.");
		return false;
	}

	// run watch thread
	if(!RunWatchThread()){
		ERR_K2HFTPRN("failed to run watch children thread");
		return false;
	}
	return true;
}

bool K2hFtPluginMan::RunWatchThread(void)
{
	if(K2hFtPluginMan::is_run_thread){
		//WAN_K2HFTPRN("Already run watch thread.");
		return true;
	}

	// run thread
	K2hFtPluginMan::thread_exit = false;
	int result;
	if(0 != (result = pthread_create(&K2hFtPluginMan::watch_thread_tid, NULL, K2hFtPluginMan::WatchChildrenExit, this))){
		ERR_K2HFTPRN("could not create watch children thread by errno(%d)", result);
		return false;
	}
	K2hFtPluginMan::is_run_thread = true;

	return true;
}

bool K2hFtPluginMan::StopWatchThread(void)
{
	if(!K2hFtPluginMan::is_run_thread){
		//WAN_K2HFTPRN("Already stop watch thread.");
		return true;
	}

	// set thread exit flag
	K2hFtPluginMan::thread_exit = true;

	// wait for exiting thread
	int		result;
	void*	pretval = NULL;
	if(0 != (result = pthread_join(K2hFtPluginMan::watch_thread_tid, &pretval))){
		ERR_K2HFTPRN("Failed to wait thread exit by errno(%d), but continue...", result);
	}
	K2hFtPluginMan::is_run_thread = false;

	return true;
}

bool K2hFtPluginMan::AddPluginInfo(PK2HFT_PLUGIN pplugin)
{
	if(!pplugin){
		ERR_K2HFTPRN("parameter pplugin is NULL.");
		return false;
	}
	if(pplugin->BaseParam.empty()){
		ERR_K2HFTPRN("pplugin->BaseParam is empty.");
		return false;
	}
	// [NOTE]
	// This is a wasteful process, but is very easy to check.
	//
	string		filename;
	strlst_t	argvs;
	strlst_t	envs;
	if(!K2hFtPluginMan::ParseExecParam(pplugin->BaseParam.c_str(), filename, argvs, envs)){
		ERR_K2HFTPRN("pplugin->BaseParam is something wrong.");
		return false;
	}
	pplugin->pid		= K2HFT_INVALID_PID;
	pplugin->pipe_input	= K2HFT_INVALID_HANDLE;
	pplugin->last_status= 0;
	pplugin->exit_count	= 0;
	pplugin->st_dev		= 0;
	pplugin->st_ino		= 0;

	// set named pipe file path
	if(!K2hFtPluginMan::BuildNamedPipeFile(pplugin->PipeFilePath)){
		ERR_K2HFTPRN("Failed to make named pipe file.");
		return false;
	}

	while(!fullock::flck_trylock_noshared_mutex(&lockval));		// MUTEX LOCK
	stop_plugin.push_back(pplugin);
	fullock::flck_unlock_noshared_mutex(&lockval);				// MUTEX UNLOCK

	MSG_K2HFTPRN("Succeed new plugin([%s] %s) into stop list(it does not run yet).", pplugin->OutputPath.empty() ? "empty" : pplugin->OutputPath.c_str(), pplugin->BaseParam.c_str());

	return true;
}

bool K2hFtPluginMan::RunPlugin(PK2HFT_PLUGIN pplugin, bool is_wait_cond)
{
	if(!pplugin){
		ERR_K2HFTPRN("parameter pplugin is NULL.");
		return false;
	}
	if(pplugin->BaseParam.empty()){
		ERR_K2HFTPRN("pplugin->BaseParam is empty.");
		return false;
	}
	if(pplugin->not_execute){
		ERR_K2HFTPRN("pplugin->not_execute is true.");
		return false;
	}

	// initialize member
	pplugin->pid			= K2HFT_INVALID_PID;
	pplugin->pipe_input		= K2HFT_INVALID_HANDLE;
	pplugin->last_status	= 0;
	pplugin->auto_restart	= true;				// force

	// check output
	if(pplugin->OutputPath.empty()){
		pplugin->st_dev	= 0;
		pplugin->st_ino	= 0;

	// cppcheck-suppress stlIfStrFind
	}else if(!mountpoint.empty() && 0 == pplugin->OutputPath.find(mountpoint)){
		// output directory is under mount point,
		// so we do not need to make directory and watch it.
		//
		MSG_K2HFTPRN("run plugin which output file(%s) under mount point, so do not make it here.", pplugin->OutputPath.c_str());
		pplugin->st_dev	= 0;
		pplugin->st_ino	= 0;

	}else{
		// output directory is not under mount point,
		// so check and make parent directory, and add watch list.
		//
		if(!pfdcache->Find(pplugin->OutputPath, pplugin->st_dev, pplugin->st_ino)){
			// make output file if it does not exist
			string	dirpath;
			if(!get_parent_dirpath(pplugin->OutputPath.c_str(), dirpath)){
				ERR_K2HFTPRN("output file(%s) is something wrong.", pplugin->OutputPath.c_str());
				return false;
			}
			// make directory
			string	abspath;
			if(!mkdir_by_abs_path(dirpath.c_str(), abspath)){
				ERR_K2HFTPRN("could not make directory(%s) to output file(%s).", dirpath.c_str(), pplugin->OutputPath.c_str());
				return false;
			}
			// check file
			if(!make_file_by_abs_path(pplugin->OutputPath.c_str(), pplugin->mode, abspath)){
				ERR_K2HFTPRN("could not make output file(%s).", pplugin->OutputPath.c_str());
				return false;
			}
			// add file watch
			if(!pfdcache->Register(pplugin->OutputPath, O_WRONLY | O_APPEND | O_CREAT, pplugin->mode, pplugin->st_dev, pplugin->st_ino)){
				ERR_K2HFTPRN("could not add watching file for output file(%s).", pplugin->OutputPath.c_str());
				return false;
			}
		}
	}

	// do run
	// cppcheck-suppress redundantAssignment
	if(K2HFT_INVALID_PID == (pplugin->pid = K2hFtPluginMan::RunPluginProcess(pplugin->BaseParam.c_str(), pplugin->OutputPath.c_str(), pplugin->PipeFilePath.c_str(), pplugin->pipe_input, (is_wait_cond ? condname.c_str() : NULL), (is_wait_cond ? mutexname.c_str() : NULL)))){
		ERR_K2HFTPRN("Could not run plugin.");
		pplugin->exit_count++;
		return false;
	}
	MSG_K2HFTPRN("Succeed run plugin([%s] %s) as pid(%d).", pplugin->OutputPath.empty() ? "empty" : pplugin->OutputPath.c_str(), pplugin->BaseParam.c_str(), pplugin->pid);

	return true;
}

bool K2hFtPluginMan::RunAddPlugin(PK2HFT_PLUGIN pplugin)
{
	// do run
	if(!RunPlugin(pplugin, false)){
		ERR_K2HFTPRN("Could not run plugin.");
		pplugin->exit_count++;
		return false;
	}

	while(!fullock::flck_trylock_noshared_mutex(&lockval));		// MUTEX LOCK

	// set running plugins list
	if(run_plugin.find(pplugin->pid) != run_plugin.end()){
		WAN_K2HFTPRN("Found pid(%d) in running plugins list, but continue...", pplugin->pid);
	}
	run_plugin[pplugin->pid] = pplugin;

	fullock::flck_unlock_noshared_mutex(&lockval);				// MUTEX UNLOCK

	return true;
}

// [NOTICE]
// If call this method after running FUSE main function(call in FUSE handler),
// Never set is_wait_cond=true.
// Probably there is possibility as deadlock.(fork in multi thread)
// So if you call with is_wait_cond=true, you must call from single thread.
//
bool K2hFtPluginMan::RunPlugins(bool is_wait_cond)
{
	while(!fullock::flck_trylock_noshared_mutex(&lockval));		// MUTEX LOCK

	// run children
	bool	result = true;
	for(k2hftpluginlist_t::iterator iter = stop_plugin.begin(); iter != stop_plugin.end(); ){
		PK2HFT_PLUGIN	pplugin = (*iter);
		if(!pplugin){
			ERR_K2HFTPRN("plugin pointer is NULL.");
			++iter;
		}else if(!pplugin->not_execute && pplugin->auto_restart){
			// do run
			if(!RunPlugin(pplugin, is_wait_cond)){
				ERR_K2HFTPRN("Could not run plugin.");
				pplugin->exit_count++;
				++iter;
				result = false;
				continue;
			}
			// set running plugins list
			if(run_plugin.find(pplugin->pid) != run_plugin.end()){
				WAN_K2HFTPRN("Found pid(%d) in running plugins list, but continue...", pplugin->pid);
			}
			run_plugin[pplugin->pid] = pplugin;

			// remove from stopping plugin list
			iter = stop_plugin.erase(iter);
		}else{
			++iter;
		}
	}
	fullock::flck_unlock_noshared_mutex(&lockval);				// MUTEX UNLOCK

	return result;
}

bool K2hFtPluginMan::ExecPlugins(void)
{
	// start(execute) all plugin
	FlShm	flobj;
	int		result;
	if(0 != (result = flobj.Broadcast(condname.c_str()))){
		ERR_K2HFTPRN("Failed to broadcast signal to condition for all plugin by errno(%d), but continue...", result);
		return false;
	}
	return true;
}

bool K2hFtPluginMan::KillPlugin(PK2HFT_PLUGIN pplugin)
{
	if(K2HFT_INVALID_PID == pplugin->pid){
		WAN_K2HFTPRN("process for path(%s) already stopped.", pplugin->OutputPath.c_str());
		return true;
	}

	// check process is running
	if(-1 == kill(pplugin->pid, 0)){
		ERR_K2HFTPRN("process(%d) does not allow signal or not exist, by errno(%d)", pplugin->pid, errno);
		return false;
	}

	// set noauto restart flag
	pplugin->auto_restart = false;

	// do stop(SIGHUP)
	if(-1 == kill(pplugin->pid, SIGHUP)){
		ERR_K2HFTPRN("process(%d) does not allow signal or not exist, by errno(%d)", pplugin->pid, errno);
		return false;
	}

	// wait for exiting(waitpid is called in watch thread)
	for(int cnt = 0; K2HFT_INVALID_PID != pplugin->pid; ++cnt){
		if(MAX_WAIT_COUNT_UNTIL_KILL == cnt){
			// do stop(SIGKILL)
			MSG_K2HFTPRN("could not stop process(%d) by SIGHUP, so try to send SIGKILL.", pplugin->pid);

			if(-1 == kill(pplugin->pid, SIGKILL)){
				ERR_K2HFTPRN("process(%d) does not allow signal or not exist, by errno(%d)", pplugin->pid, errno);
			}

		}else if((MAX_WAIT_COUNT_UNTIL_KILL * 2) < cnt){
			ERR_K2HFTPRN("gave up to stop process(%d)", pplugin->pid);
			return false;
		}
		// sleep
		struct timespec	sleeptime = {0L, 1 * 1000 * 1000};		// wait 1ms
		nanosleep(&sleeptime, NULL);
	}
	MSG_K2HFTPRN("stopped plugin(%d).", pplugin->pid);

	return true;
}

bool K2hFtPluginMan::StopPlugin(PK2HFT_PLUGIN pplugin)
{
	if(!pplugin){
		ERR_K2HFTPRN("parameter pplugin is NULL.");
		return false;
	}
	if(pplugin->not_execute){
		ERR_K2HFTPRN("pplugin->not_execute is true.");
		return false;
	}
	if(K2HFT_INVALID_PID == pplugin->pid){
		WAN_K2HFTPRN("pplugin->pid is not running status, so already stop.");
		return true;
	}

	while(!fullock::flck_trylock_noshared_mutex(&lockval));		// MUTEX LOCK

	// check plugin is running
	bool	is_found = false;
	for(k2hftpluginmap_t::iterator iter = run_plugin.begin(); iter != run_plugin.end(); ++iter){
		if(iter->second == pplugin){
			is_found = true;
			break;
		}
	}
	fullock::flck_unlock_noshared_mutex(&lockval);				// MUTEX UNLOCK

	if(!is_found){
		WAN_K2HFTPRN("pplugin->pid is not running status, so already stop.");
		return true;
	}

	// kill process(with setting noauto restart flag)
	if(!KillPlugin(pplugin)){
		ERR_K2HFTPRN("could not stop(kill) process(%d), but continue...", pplugin->pid);
	}else{
		MSG_K2HFTPRN("Succeed stopping plugin([%s] %s)", pplugin->OutputPath.empty() ? "empty" : pplugin->OutputPath.c_str(), pplugin->BaseParam.c_str());
	}

	return true;
}

bool K2hFtPluginMan::StopPlugins(bool force)
{
	// make plugin list for temporary
	while(!fullock::flck_trylock_noshared_mutex(&lockval));		// MUTEX LOCK

	k2hftpluginlist_t		tmplist_plugin;
	for(k2hftpluginmap_t::iterator iter = run_plugin.begin(); iter != run_plugin.end(); ++iter){
		tmplist_plugin.push_back(iter->second);
	}
	fullock::flck_unlock_noshared_mutex(&lockval);				// MUTEX UNLOCK

	// loop for stop all plugins
	bool	is_error = false;
	for(k2hftpluginlist_t::iterator iter = tmplist_plugin.begin(); iter != tmplist_plugin.end(); ++iter){
		PK2HFT_PLUGIN	pplugin = (*iter);

		// kill process(with setting noauto restart flag)
		if(!KillPlugin(pplugin)){
			ERR_K2HFTPRN("could not stop(kill) process(%d), but continue...", pplugin->pid);

			// force set
			pplugin->pid		= K2HFT_INVALID_PID;
			pplugin->last_status= -1;		// means error
			pplugin->exit_count++;

			while(!fullock::flck_trylock_noshared_mutex(&lockval));	// MUTEX LOCK
			stop_plugin.push_back(pplugin);
			fullock::flck_unlock_noshared_mutex(&lockval);			// MUTEX UNLOCK

			// [NOTE]
			// if error, removing plugin pointer from run list after this loop.
			//
			is_error = true;

		}else{
			MSG_K2HFTPRN("Succeed stopping plugin([%s] %s) and adding stop list.", pplugin->OutputPath.empty() ? "empty" : pplugin->OutputPath.c_str(), pplugin->BaseParam.c_str());
		}

		// [NOTICE]
		// close plugin's named pipe handles.
		//
		K2HFT_CLOSE(pplugin->pipe_input);
	}

	// check run plugin list if error is occurred.
	if(is_error){
		while(!fullock::flck_trylock_noshared_mutex(&lockval));	// MUTEX LOCK

		for(k2hftpluginmap_t::iterator iter = run_plugin.begin(); iter != run_plugin.end(); ){
			PK2HFT_PLUGIN	pplugin = iter->second;
			if(K2HFT_INVALID_PID == pplugin->pid){
				run_plugin.erase(iter++);
			}else{
				++iter;
			}
		}
		fullock::flck_unlock_noshared_mutex(&lockval);			// MUTEX UNLOCK
	}
	return true;
}

bool K2hFtPluginMan::Write(PK2HFT_PLUGIN pplugin, unsigned char* pdata, size_t length)
{
	if(!pplugin || !pdata || 0 == length){
		ERR_K2HFTPRN("parameters are wrong.");
		return false;
	}

	// check output
	// cppcheck-suppress stlIfStrFind
	if(!pplugin->OutputPath.empty() && (mountpoint.empty() || 0 != pplugin->OutputPath.find(mountpoint))){
		if(!pfdcache->Find(pplugin->OutputPath, pplugin->st_dev, pplugin->st_ino)){
			// output file is changed or removed, so need to restart plugin.

			// stop plugin
			if(!StopPlugin(pplugin)){
				ERR_K2HFTPRN("could not stop plugin, we must stop plugin because output file is changed.");
				return false;
			}

			// run plugin
			if(!RunPlugin(pplugin, false)){
				ERR_K2HFTPRN("Could not run plugin.");
				return false;
			}

			// set running plugins list
			while(!fullock::flck_trylock_noshared_mutex(&lockval));	// MUTEX LOCK

			if(run_plugin.end() != run_plugin.find(pplugin->pid)){
				WAN_K2HFTPRN("Found pid(%d) in running plugins list, but continue...", pplugin->pid);
			}
			run_plugin[pplugin->pid] = pplugin;

			fullock::flck_unlock_noshared_mutex(&lockval);			// MUTEX UNLOCK
		}
	}

	// check input pipe
	if(K2HFT_INVALID_HANDLE == pplugin->pipe_input){
		ERR_K2HFTPRN("The input pipe for plugin is invalid, could not write data to plugin.");
		return false;
	}

	fullock::flck_lock_noshared_mutex(&(pplugin->lockval));			// MUTEX LOCK

	// do write in loop
	bool			result		= false;
	int				errno_bup	= 0;
	struct timespec	sleeptime	= {0L, 100 * 1000};	// 100us

	for(int trycnt = 0; trycnt < K2hFtPluginMan::WRITE_RETRY_MAX; trycnt++){
		if(K2HFT_INVALID_HANDLE == pplugin->pipe_input){
			nanosleep(&sleeptime, NULL);
			continue;
		}

		// do write
		errno_bup = 0;
		if(true == (result = k2hft_write(pplugin->pipe_input, pdata, length))){
			// succeed
			break;
		}
		errno_bup = errno;

		if(EPIPE == errno_bup){
			WAN_K2HFTPRN("Could not write to plugin input pipe(%d) by errno(EPIPE: %d), maybe plugin is down, so retry...", pplugin->pipe_input, errno_bup);
		}else{
			WAN_K2HFTPRN("Could not write to plugin input pipe(%d) by errno(%d), but retry...", pplugin->pipe_input, errno_bup);
		}
		nanosleep(&sleeptime, NULL);
	}

	fullock::flck_unlock_noshared_mutex(&(pplugin->lockval));	// MUTEX UNLOCK

	if(!result && 0 != errno_bup){
		ERR_K2HFTPRN("Could not write to plugin input pipe(%d) by errno(%d)", pplugin->pipe_input, errno_bup);
	}
	return result;
}

/*
 * VIM modelines
 *
 * vim:set ts=4 fenc=utf-8:
 */
