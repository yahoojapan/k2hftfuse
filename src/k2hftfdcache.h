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
 * CREATE:   Tue Oct 27 2015
 * REVISION:
 *
 */

#ifndef	K2HFTFDCACHE_H
#define K2HFTFDCACHE_H

#include <signal.h>
#include <string>
#include <map>
#include <fullock/flckstructure.h>
#include <fullock/flckbaselist.tcc>

//---------------------------------------------------------
// Structure
//---------------------------------------------------------
typedef struct k2hft_file_watch_info{
	std::string		filepath;				// file path
	int				inotify_fd;				// inotify fd
	int				watch_fd;				// watch fd for epoll
	int				opened_fd;				// opened fd
	dev_t			st_dev;					// device id at opening
	ino_t			st_ino;					// inode id at opening

	k2hft_file_watch_info() : filepath(""), inotify_fd(K2HFT_INVALID_HANDLE), watch_fd(K2HFT_INVALID_HANDLE), opened_fd(K2HFT_INVALID_HANDLE), st_dev(0), st_ino(0) {}
}K2HFTFW, *PK2HFTFW;

typedef	std::map<std::string, PK2HFTFW>		k2hftfwmap_t;

//---------------------------------------------------------
// Class K2hFtFdCache
//---------------------------------------------------------
// [NOTE]
//
// This class is watching file which is removed or moved by inotify.
// So this does not support for the files on network device, special
// drive(/proc, etc), and fuse drive.
// 
// inotify man page:
// Inotify reports only events that a user-space program triggers
// through the filesystem API.  As a result, it does not catch remote
// events that occur on network filesystems.  (Applications must fall
// back to polling the filesystem to catch such events.)  Furthermore,
// various pseudo-filesystems such as /proc, /sys, and /dev/pts are not
// monitorable with inotify.
// 
class K2hFtFdCache
{
	protected:
		static const int	SIGNUM			= SIGUSR2;
		static const int	WAIT_EVENT_MAX	= 32;		// wait event max count

		volatile int		lockval;					// the value to curfwmap(fullock_mutex)
		volatile bool		run_thread;
		int					epollfd;
		pthread_t			watch_thread;
		k2hftfwmap_t		curfwmap;

	protected:
		static int MakeSignalFd(void);
		static bool CheckInotifyEvents(int inotifyfd, K2hFtFdCache* pFdCache);
		static void* WorkerThread(void* param);

		bool DeleteFileWatchEpoll(PK2HFTFW pfw);
		bool AddFileWatchEpoll(PK2HFTFW pfw);
		bool AddAllFileWatchEpoll(void);

		// [NOTE]
		// This method returns READER LOCKed K2HFTFW
		//
		PK2HFTFW GetFileWatch(const std::string& filepath, int openflags, mode_t openmode);
		bool DeleteFileWatch(int inotifyfd);
		bool CheckRemoveFileWatch(int inotifyfd);

		bool KickSignal(void);

	public:
		K2hFtFdCache(void);
		virtual ~K2hFtFdCache(void);

		bool Initialize(void);
		bool Close(void);

		bool Register(const std::string& filepath, int openflags, mode_t openmode, dev_t& st_dev, ino_t& st_ino);
		bool Find(const std::string& filepath, dev_t st_dev, ino_t st_ino);
		bool Write(const std::string& filepath, int openflags, mode_t openmode, unsigned char* pdata, size_t length);
};

#endif	// K2HFTFDCACHE_H

/*
 * VIM modelines
 *
 * vim:set ts=4 fenc=utf-8:
 */
