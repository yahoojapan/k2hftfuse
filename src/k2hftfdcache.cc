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
 * CREATE:   Tue Oct 27 2015
 * REVISION:
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/inotify.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/signalfd.h>

#include <fullock/fullock.h>

#include "k2hftcommon.h"
#include "k2hftfdcache.h"
#include "k2hftutil.h"
#include "k2hftdbg.h"

using namespace std;

//---------------------------------------------------------
// K2hFtFdCache : Class variable
//---------------------------------------------------------
const int	K2hFtFdCache::SIGNUM;
const int	K2hFtFdCache::WAIT_EVENT_MAX;

//---------------------------------------------------------
// K2hFtFdCache : Class Methods
//---------------------------------------------------------
int K2hFtFdCache::MakeSignalFd(void)
{
	// set signal mask
	sigset_t	sigset;
	if(-1 == sigemptyset(&sigset) || -1 == sigaddset(&sigset, K2hFtFdCache::SIGNUM)){
		ERR_K2HFTPRN("Could not allow %d signal by errno(%d)", K2hFtFdCache::SIGNUM, errno);
		return K2HFT_INVALID_HANDLE;
	}

	int	result;
	if(0 != (result = pthread_sigmask(SIG_SETMASK, &sigset, NULL))){
		ERR_K2HFTPRN("Could not allow %d signal by errno(%d)", K2hFtFdCache::SIGNUM, result);
		return K2HFT_INVALID_HANDLE;
	}

	// make signal fd
	int	sigfd;
	if(K2HFT_INVALID_HANDLE == (sigfd = signalfd(-1, &sigset, SFD_NONBLOCK | SFD_CLOEXEC))){
		ERR_K2HFTPRN("Could not make signalfd for %d signal by errno(%d)", K2hFtFdCache::SIGNUM, errno);
		return K2HFT_INVALID_HANDLE;
	}
	return sigfd;
}

//
// If move and remove event, returns true.
//
bool K2hFtFdCache::CheckInotifyEvents(int inotifyfd, K2hFtFdCache* pFdCache)
{
	if(!pFdCache){
		ERR_K2HFTPRN("parameter is wrong");
		return false;
	}

	// Loop
	for(;;){
		// [NOTICE]
		// Some systems cannot read integer variables if they are not properly aligned.
		// On other systems, incorrect alignment may decrease performance.
		// Hence, the buffer used for reading from the inotify file descriptor should 
		// have the same alignment as struct inotify_event.
		//
		unsigned char	buff[4096] __attribute__ ((aligned(__alignof__(struct inotify_event))));
		ssize_t			length;

		memset(buff, 0, sizeof(buff));

		// read from inotify event[s]
		if(-1 == (length = read(inotifyfd, buff, sizeof(buff)))){
			if(EAGAIN == errno){
				//MSG_K2HFTPRN("reach to EAGAIN.");
				return false;
			}
			WAN_K2HFTPRN("errno(%d) occurred during reading from inotify fd(%d)", errno, inotifyfd);
			return false;
		}
		if(0 >= length){
			//MSG_K2HFTPRN("no more inotify event data.");
			return false;
		}

		// loop by each inotify_event
		const struct inotify_event*	in_event;
		for(unsigned char* ptr = buff; ptr < (buff + length); ptr += sizeof(struct inotify_event) + in_event->len){
			in_event = (const struct inotify_event*)ptr;

			if(in_event->mask & IN_DELETE || in_event->mask & IN_DELETE_SELF || in_event->mask & IN_MOVE_SELF || in_event->mask & IN_MOVED_FROM || in_event->mask & IN_MOVED_TO){
				// remove or move the file
				return true;
			}else if(in_event->mask & IN_ATTRIB){
				// check file exists
				return pFdCache->CheckRemoveFileWatch(inotifyfd);
			}
		}
	}
	return false;
}

void* K2hFtFdCache::WorkerThread(void* param)
{
	K2hFtFdCache*	pFdCache = reinterpret_cast<K2hFtFdCache*>(param);
	if(!pFdCache){
		ERR_K2HFTPRN("paraemter is wrong.");
		pthread_exit(NULL);
	}

	// set signal fd for main thread
	int	sigfd;
	if(K2HFT_INVALID_HANDLE == (sigfd = K2hFtFdCache::MakeSignalFd())){
		ERR_K2HFTPRN("could not set signal.");
		pthread_exit(NULL);
	}

	// add signal fd to epoll event
	struct epoll_event	epoolev;
	memset(&epoolev, 0, sizeof(struct epoll_event));
	epoolev.data.fd	= sigfd;
	epoolev.events	= EPOLLIN;
	if(-1 == epoll_ctl(pFdCache->epollfd, EPOLL_CTL_ADD, sigfd, &epoolev)){
		ERR_K2HFTPRN("Could not set signal fd(%d) to epoll fd(%d) by errno(%d)", sigfd, pFdCache->epollfd, errno);
		pthread_exit(NULL);
	}

	while(pFdCache->run_thread){
		// check & add events
		if(!pFdCache->AddAllFileWatchEpoll()){
			ERR_K2HFTPRN("Failed to check and add epoll events.");
			break;
		}

		// wait events
		struct epoll_event  events[WAIT_EVENT_MAX];
		int					eventcnt;
		if(0 < (eventcnt = epoll_pwait(pFdCache->epollfd, events, WAIT_EVENT_MAX, -1, NULL))){
			// catch event
			for(int cnt = 0; cnt < eventcnt; cnt++){
				if(events[cnt].data.fd == sigfd){
					// read signal info from signal fd for remove signal
					struct signalfd_siginfo		signalinfo;
					ssize_t						silength;
					memset(&signalinfo, 0, sizeof(struct signalfd_siginfo));
					silength = read(sigfd, &signalinfo, sizeof(struct signalfd_siginfo));

					if(silength == sizeof(struct signalfd_siginfo)){
						if(signalinfo.ssi_signo != static_cast<unsigned int>(K2hFtFdCache::SIGNUM)){
							WAN_K2HFTPRN("Caught signal from signal fd(%d) is %u signal(expected %d signal), but continue...", sigfd, signalinfo.ssi_signo, K2hFtFdCache::SIGNUM);
						}
					}else{
						WAN_K2HFTPRN("the result length(%zd) which is read from signal fd(%d) is wrong length, errno is %d.", silength, sigfd, errno);
					}

					// [NOTE]
					// signal fd is for break watch events, so to do nothing here.
					//
					if(!pFdCache->run_thread){
						// exit as soon as possible
						break;
					}
				}else{
					// check removing/moving file
					if(K2hFtFdCache::CheckInotifyEvents(events[cnt].data.fd, pFdCache)){
						// check all inotify fd
						if(!pFdCache->DeleteFileWatch(events[cnt].data.fd)){
							WAN_K2HFTPRN("There is not same inotify fd(%d) in mapping, but continue...", events[cnt].data.fd);
						}else{
							MSG_K2HFTPRN("Caught inotify event(remove or move) for inotifyfd(%d), and succeed deleting file watch.", events[cnt].data.fd);
						}
					}
				}
			}

		}else if(-1 >= eventcnt){
			if(EINTR != errno){
				ERR_K2HFTPRN("Something error occured in waiting event fd(%d) by errno(%d)", pFdCache->epollfd, errno);
				break;
			}
			// signal occurred.

		}else{	// 0 == eventcnt
			// timeouted, nothing to do
		}
	}

	// remove signal fd
	if(-1 == epoll_ctl(pFdCache->epollfd, EPOLL_CTL_DEL, sigfd, NULL)){
		ERR_K2HFTPRN("Fialed to remove signal fd(%d) from epoll fd(%d) by errno(%d), but continue...", sigfd, pFdCache->epollfd, errno);
	}
	K2HFT_CLOSE(sigfd);

	pthread_exit(NULL);
	return NULL;
}

//---------------------------------------------------------
// K2hFtFdCache : Methods
//---------------------------------------------------------
K2hFtFdCache::K2hFtFdCache(void) : lockval(FLCK_NOSHARED_MUTEX_VAL_UNLOCKED), run_thread(false), epollfd(K2HFT_INVALID_HANDLE)
{
}

K2hFtFdCache::~K2hFtFdCache(void)
{
	Close();
}

bool K2hFtFdCache::Initialize(void)
{
	// create epoll fd
	if(K2HFT_INVALID_HANDLE == epollfd){
		if(K2HFT_INVALID_HANDLE == (epollfd = epoll_create1(EPOLL_CLOEXEC))){
			ERR_K2HFTPRN("Could not make epoll fd by errno(%d).", errno);
			Close();
			return false;
		}
	}

	// run thread
	if(!run_thread){
		run_thread = true;

		int	result;
		if(0 != (result = pthread_create(&watch_thread, NULL, K2hFtFdCache::WorkerThread, this))){
			ERR_K2HFTPRN("Could not create thread by errno(%d)", result);
			Close();
			return false;
		}
	}
	return true;
}

bool K2hFtFdCache::Close(void)
{
	// exiting thread
	if(run_thread){
		run_thread = false;		// for stop

		// kick signal
		if(!K2hFtFdCache::KickSignal()){
			ERR_K2HFTPRN("Could not kick signal, but continue...");
		}else{
			// wait for thread exit
			void*	pretval = NULL;
			int		result;
			if(0 != (result = pthread_join(watch_thread, &pretval))){
				ERR_K2HFTPRN("Failed to wait thread exiting by errno(%d), but continue,,,", result);
			}else{
				MSG_K2HFTPRN("Succeed to wait thread exiting,");
			}
		}
	}

	// close all inotify fds
	for(k2hftfwmap_t::iterator iter = curfwmap.begin(); iter != curfwmap.end(); ++iter){
		// no care for lockval
		PK2HFTFW	pfw = iter->second;
		DeleteFileWatchEpoll(pfw);
	}
	curfwmap.clear();

	K2HFT_CLOSE(epollfd);

	return true;
}

bool K2hFtFdCache::DeleteFileWatchEpoll(PK2HFTFW pfw)
{
	if(!pfw){
		ERR_K2HFTPRN("pfw is NULL");
		return false;
	}

	if(K2HFT_INVALID_HANDLE != epollfd && K2HFT_INVALID_HANDLE != pfw->inotify_fd){
		if(-1 == epoll_ctl(epollfd, EPOLL_CTL_DEL, pfw->inotify_fd, NULL)){
			ERR_K2HFTPRN("Fialed to remove inotify fd(%d) from epoll fd(%d) by errno(%d), but continue...", pfw->inotify_fd, epollfd, errno);
		}
	}
	if(K2HFT_INVALID_HANDLE != pfw->inotify_fd && K2HFT_INVALID_HANDLE != pfw->watch_fd){
		if(-1 == inotify_rm_watch(pfw->inotify_fd, pfw->watch_fd)){
			ERR_K2HFTPRN("Fialed to remove watch fd(%d) from inotify fd(%d) by errno(%d), but continue...", pfw->watch_fd, pfw->inotify_fd, errno);
		}
	}
	FLCK_CLOSE(pfw->watch_fd);
	FLCK_CLOSE(pfw->inotify_fd);
	K2HFT_CLOSE(pfw->opened_fd);

	MSG_K2HFTPRN("Succeed deleting file watch for %s", pfw->filepath.c_str());

	// no care for lockval
	K2HFT_DELETE(pfw);

	return true;
}

bool K2hFtFdCache::AddFileWatchEpoll(PK2HFTFW pfw)
{
	if(!pfw){
		ERR_K2HFTPRN("pfw is NULL");
		return false;
	}
	if(K2HFT_INVALID_HANDLE == epollfd){
		ERR_K2HFTPRN("epoll fd is not initialized.");
		return false;
	}
	if(K2HFT_INVALID_HANDLE != pfw->inotify_fd){
		MSG_K2HFTPRN("watch fd(%d) and inotify fd(%d) is already set to epoll.", pfw->watch_fd, pfw->inotify_fd);
		return true;
	}

	// create inotify
	if(K2HFT_INVALID_HANDLE == (pfw->inotify_fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC))){
		ERR_K2HFTPRN("Could not create inotify fd for %s by errno(%d).", pfw->filepath.c_str(), errno);
		return false;
	}

	// add watching path
	//
	// [NOTE]
	// IN_DELETE(_SELF) does not occur if the file is removied. :-(
	// So we use IN_ATTRIB instead of it.
	//
	if(K2HFT_INVALID_HANDLE == (pfw->watch_fd = inotify_add_watch(pfw->inotify_fd, pfw->filepath.c_str(), IN_DELETE | IN_MOVE | IN_DELETE_SELF | IN_MOVE_SELF | IN_ATTRIB))){
		ERR_K2HFTPRN("Could not add inotify fd(%d) to watch fd for %s by errno(%d).", pfw->inotify_fd, pfw->filepath.c_str(), errno);
		K2HFT_CLOSE(pfw->inotify_fd);
		return false;
	}

	// set epoll
	struct epoll_event	epoolev;
	memset(&epoolev, 0, sizeof(struct epoll_event));
	epoolev.data.fd	= pfw->inotify_fd;
	epoolev.events	= EPOLLIN | EPOLLET;

	if(-1 == epoll_ctl(epollfd, EPOLL_CTL_ADD, pfw->inotify_fd, &epoolev)){
		ERR_K2HFTPRN("Could not add inotify fd(%d) and watch fd(%d) to epoll fd(%d) for %s by errno(%d).", pfw->inotify_fd, pfw->watch_fd, epollfd, pfw->filepath.c_str(), errno);

		if(-1 == inotify_rm_watch(pfw->inotify_fd, pfw->watch_fd)){
			ERR_K2HFTPRN("Fialed to remove watch fd(%d) from inotify fd(%d) by errno(%d), but continue...", pfw->watch_fd, pfw->inotify_fd, errno);
		}
		K2HFT_CLOSE(pfw->watch_fd);
		K2HFT_CLOSE(pfw->inotify_fd);

		return false;
	}
	MSG_K2HFTPRN("Succeed adding new file watch for %s", pfw->filepath.c_str());

	return true;
}

bool K2hFtFdCache::AddAllFileWatchEpoll(void)
{
	if(!run_thread){
		ERR_K2HFTPRN("Why call this method when thread does not run.");
		return false;
	}
	if(K2HFT_INVALID_HANDLE == epollfd){
		ERR_K2HFTPRN("epoll fd is not initialized.");
		return false;
	}

	// LOCK
	while(!fullock::flck_trylock_noshared_mutex(&lockval));		// MUTEX LOCK

	// add all inotify fds
	for(k2hftfwmap_t::iterator iter = curfwmap.begin(); iter != curfwmap.end(); ++iter){
		PK2HFTFW	pfw = iter->second;

		// add events
		if(!AddFileWatchEpoll(pfw)){							// NOTICE: not care for lockval
			ERR_K2HFTPRN("Fialed to add epoll event, but continue...");
		}
	}
	fullock::flck_unlock_noshared_mutex(&lockval);				// MUTEX UNLOCK

	return true;
}

bool K2hFtFdCache::DeleteFileWatch(int inotifyfd)
{
	if(K2HFT_INVALID_HANDLE == inotifyfd){
		ERR_K2HFTPRN("inotify fd is empty.");
		return false;
	}

	// LOCK
	while(!fullock::flck_trylock_noshared_mutex(&lockval));		// MUTEX LOCK

	for(k2hftfwmap_t::iterator iter = curfwmap.begin(); iter != curfwmap.end(); ++iter){
		PK2HFTFW	pfw = iter->second;
		if(!pfw){
			continue;
		}
		if(pfw->inotify_fd == inotifyfd){
			// need to close file

			// remove all
			DeleteFileWatchEpoll(pfw);

			// remove from mapping
			curfwmap.erase(iter);

			fullock::flck_unlock_noshared_mutex(&lockval);		// MUTEX UNLOCK

			return true;
		}
	}
	fullock::flck_unlock_noshared_mutex(&lockval);				// MUTEX UNLOCK

	return false;
}

bool K2hFtFdCache::CheckRemoveFileWatch(int inotifyfd)
{
	if(K2HFT_INVALID_HANDLE == inotifyfd){
		ERR_K2HFTPRN("inotify fd is empty.");
		return false;
	}

	// LOCK
	while(!fullock::flck_trylock_noshared_mutex(&lockval));		// MUTEX LOCK

	for(k2hftfwmap_t::iterator iter = curfwmap.begin(); iter != curfwmap.end(); ++iter){
		PK2HFTFW	pfw = iter->second;
		if(!pfw){
			continue;
		}
		if(pfw->inotify_fd == inotifyfd){
			fullock::flck_unlock_noshared_mutex(&lockval);		// MUTEX UNLOCK

			// check file existing
			struct stat	st;
			if(-1 == stat(pfw->filepath.c_str(), &st)){
				MSG_K2HFTPRN("Could not get stat for %s(errno:%d), so the file is removed or moved.", pfw->filepath.c_str(), errno);
				return true;
			}
			if(pfw->st_dev != st.st_dev || pfw->st_ino != st.st_ino){
				MSG_K2HFTPRN("devid(%lu)/inodeid(%lu) for %s is not same as in cache( devid(%lu)/inodeid(%lu) )", st.st_dev, st.st_ino, pfw->filepath.c_str(), pfw->st_dev, pfw->st_ino);
				return true;
			}
			return false;
		}
	}
	fullock::flck_unlock_noshared_mutex(&lockval);				// MUTEX UNLOCK

	return false;
}

//
// This method locks lockval mutex for k2hftfwmap_t.
// And after that, locks lockval for each K2HFTFW.
//
PK2HFTFW K2hFtFdCache::GetFileWatch(const string& filepath, int openflags, mode_t openmode)
{
	if(filepath.empty()){
		ERR_K2HFTPRN("filepath is empty.");
		return NULL;
	}

	// LOCK
	while(!fullock::flck_trylock_noshared_mutex(&lockval));		// MUTEX LOCK

	k2hftfwmap_t::iterator	iter;
	PK2HFTFW				pfw;
	if(curfwmap.end() != (iter = curfwmap.find(filepath))){
		pfw = iter->second;
		if(pfw){
			if(K2HFT_INVALID_HANDLE == pfw->opened_fd){
				// need to open file
				if(K2HFT_INVALID_HANDLE == (pfw->opened_fd = open(pfw->filepath.c_str(), openflags, openmode))){
					ERR_K2HFTPRN("Could not create/open output file(%s) by errno(%d).", filepath.c_str(), errno);
					fullock::flck_unlock_noshared_mutex(&lockval);	// MUTEX UNLOCK
					return NULL;
				}

				// device/inode id
				struct stat	st;
				if(-1 != fstat(pfw->opened_fd, &st)){
					pfw->st_dev = st.st_dev;
					pfw->st_ino = st.st_ino;
				}else{
					ERR_K2HFTPRN("Could not get stat for output file(%s) by errno(%d).", filepath.c_str(), errno);
					pfw->st_dev = 0;
					pfw->st_ino = 0;
				}
				MSG_K2HFTPRN("Succeed open file %s, before adding file watch yet.", pfw->filepath.c_str());
			}
			fullock::flck_unlock_noshared_mutex(&lockval);		// MUTEX UNLOCK
			return pfw;
		}else{
			ERR_K2HFTPRN("WHY: PK2HFTFW pointer for %s is NULL, try to recover...", filepath.c_str());
			curfwmap.erase(iter);
		}
	}

	// initialize
	pfw				= new K2HFTFW;
	pfw->filepath	= filepath;

	// open new file
	if(K2HFT_INVALID_HANDLE == (pfw->opened_fd = open(pfw->filepath.c_str(), openflags, openmode))){
		ERR_K2HFTPRN("Could not create/open output file(%s) by errno(%d).", filepath.c_str(), errno);
		fullock::flck_unlock_noshared_mutex(&lockval);			// MUTEX UNLOCK
		K2HFT_DELETE(pfw);
		return NULL;
	}

	// device/inode id
	struct stat	st;
	if(-1 != fstat(pfw->opened_fd, &st)){
		pfw->st_dev = st.st_dev;
		pfw->st_ino = st.st_ino;
	}else{
		ERR_K2HFTPRN("Could not get stat for output file(%s) by errno(%d).", filepath.c_str(), errno);
		pfw->st_dev = 0;
		pfw->st_ino = 0;
	}
	MSG_K2HFTPRN("Succeed open file %s, before adding file watch yet.", pfw->filepath.c_str());

	// add mapping
	curfwmap[filepath] = pfw;

	fullock::flck_unlock_noshared_mutex(&lockval);				// MUTEX UNLOCK

	// kick signal to thread
	//
	// [NOTE]
	// Added K2HFTFW is set inotify in thread.
	//
	if(!K2hFtFdCache::KickSignal()){
		ERR_K2HFTPRN("Could not kick signal, but continue...");
	}
	return pfw;
}

bool K2hFtFdCache::Regist(const string& filepath, int openflags, mode_t openmode, dev_t& st_dev, ino_t& st_ino)
{
	PK2HFTFW	pfw;
	if(NULL == (pfw = GetFileWatch(filepath, openflags, openmode))){
		ERR_K2HFTPRN("Could not get file watch for file(%s)", filepath.c_str());
		return false;
	}
	st_dev = pfw->st_dev;
	st_ino = pfw->st_ino;

	return true;
}

bool K2hFtFdCache::Find(const string& filepath, dev_t st_dev, ino_t st_ino)
{
	if(filepath.empty()){
		ERR_K2HFTPRN("filepath is empty.");
		return false;
	}

	// LOCK
	while(!fullock::flck_trylock_noshared_mutex(&lockval));		// MUTEX LOCK

	k2hftfwmap_t::iterator	iter;
	bool					result = false;
	if(curfwmap.end() != (iter = curfwmap.find(filepath))){
		if(iter->second && (iter->second)->st_dev == st_dev && (iter->second)->st_ino == st_ino){
			result = true;
		}
	}
	fullock::flck_unlock_noshared_mutex(&lockval);				// MUTEX UNLOCK

	return result;
}

bool K2hFtFdCache::Write(const string& filepath, int openflags, mode_t openmode, unsigned char* pdata, size_t length)
{
	PK2HFTFW	pfw;
	int			fd;
	int			direct_fd = K2HFT_INVALID_HANDLE;
	if(NULL == (pfw = GetFileWatch(filepath, openflags, openmode))){
		WAN_K2HFTPRN("Could not open file(%s) by caching fd method, but try to open directly", filepath.c_str());

		if(K2HFT_INVALID_HANDLE == (direct_fd = open(filepath.c_str(), openflags, openmode))){
			ERR_K2HFTPRN("Could not create/open output file(%s) by errno(%d).", filepath.c_str(), errno);
			return false;
		}
		fd = direct_fd;
	}else{
		fd = pfw->opened_fd;
	}

	// Lock
	fullock_rwlock_wrlock(fd, 0, 1);					// WRITE LOCK

	// [NOTE]
	// do not need to seek to end of file, because of opening file with O_APPEND
	//
	bool	result;
	if(false == (result = k2hft_write(fd, pdata, length))){
		ERR_K2HFTPRN("Could not write to output file(%s) by errno(%d).", filepath.c_str(), errno);
	}else{
		// [NOTE]
		// do not sync the file for performance.
		//
		//fdatasync(fd);
	}

	fullock_rwlock_unlock(fd, 0, 1);					// UNLOCK

	if(K2HFT_INVALID_HANDLE != direct_fd){
		K2HFT_CLOSE(direct_fd);
	}
	return result;
}

bool K2hFtFdCache::KickSignal(void)
{
	// kick signal
	int	result;
	if(0 != (result = pthread_kill(watch_thread, K2hFtFdCache::SIGNUM))){
		ERR_K2HFTPRN("Could not kick signal %d by errno(%d).", K2hFtFdCache::SIGNUM, result);
		return false;
	}
	return true;
}

/*
 * VIM modelines
 *
 * vim:set ts=4 fenc=utf-8:
 */
