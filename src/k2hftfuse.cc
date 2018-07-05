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
 * CREATE:   Tue Sep 1 2015
 * REVISION:
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <assert.h>
#include <dirent.h>
#include <pwd.h>
#include <grp.h>
#include <k2hash/k2hash.h>
#include <k2hash/k2htransfunc.h>
#include <chmpx/chmpx.h>
#include <chmpx/chmconfutil.h>
#include <fuse.h>			// Need to define FUSE_USE_VERSION=26 before including fuse.h

#include <string>

#include "k2hftcommon.h"
#include "k2hftman.h"
#include "k2hftutil.h"
#include "k2hftdbg.h"

using namespace std;

//---------------------------------------------------------
// Symbols
//---------------------------------------------------------
#define	K2HFT_FUSERMOUNT_CMDLINE_PREFIX		"/bin/fusermount -u "
#define	K2HFT_FUSERMOUNT_CMDLINE_SUFIX		" > /dev/null 2>&1 &"

// Environment
#define	K2HFT_CONFFILE_ENV_NAME				"K2HFTCONFFILE"
#define	K2HFT_JSONCONF_ENV_NAME				"K2HFTJSONCONF"

//---------------------------------------------------------
// Variables
//---------------------------------------------------------
static K2hFtManage*	pK2hFtMan				= NULL;
static bool			help_mode				= false;
static bool			version_mode			= false;
static bool			is_run_chmpx			= false;			// default: do not run chmpx

// for options
static mode_t		opt_umask				= 0;
static uid_t		opt_uid					= 0;
static gid_t		opt_gid					= 0;
static bool			is_set_umask			= false;
static bool			is_set_uid				= false;
static bool			is_set_gid				= false;

// [NOTE]
// To avoid static object initialization order problem(SIOF)
//
static string& GetMountPoint(void)
{
	static string	mountpoint("");
	return mountpoint;
}

static string& GetConfig(void)
{
	static string	config("");
	return config;
}

static string& GetChmpxLog(void)
{
	static string	chmpxlog("");
	return chmpxlog;
}

//---------------------------------------------------------
// Version & Usage
//---------------------------------------------------------
extern char k2hftfuse_commit_hash[];

static void k2hftfuse_print_version(FILE* stream)
{
	static const char format[] =
		"\n"
		"k2hftfuse Version %s (commit: %s)\n"
		"\n"
		"Copyright(C) 2015 Yahoo Japan Corporation.\n"
		"\n"
		"k2hftfuse is file transaction system on FUSE file system with\n"
		"K2HASH and K2HASH TRANSACTION PLUGIN, CHMPX.\n"
		"\n";

	if(!stream){
		stream = stdout;
	}
	fprintf(stream, format, VERSION, k2hftfuse_commit_hash);

	k2h_print_version(stream);
	chmpx_print_version(stream);
}

static void k2hftfuse_print_usage(FILE* stream)
{
	if(!stream){
		stream = stdout;
	}
	fprintf(stream,	"[Usage] k2hftfuse\n"
					" You can run k2hftfuse by two way, one is manual and the other is using mount command.\n"
					"\n"
					" * run k2hftfuse manually\n"
					"     k2hftfuse [mount point] [k2hftfuse or fuse options]\n"
					"   for example:\n"
					"     $ k2hftfuse /mnt/k2hfs -o allow_other,nodev,nosuid,_netdev,dbglevel=err,conf=/etc/k2hftfuse.conf -f -d &\n"
					"\n"
					" * using mount/umount/fusermount\n"
					"   makes following formatted line in fstab:\n"
					"     k2hftfuse [mount point] fuse [k2hftfuse or fuse options]\n"
					"   for example:\n"
					"     k2hftfuse /mnt/k2hfs fuse allow_other,nodev,nosuid,_netdev,dbglevel=err,conf=/etc/k2hftfuse.conf 0 0\n"
					"   you can run k2hftfuse by mount command.\n"
					"     $ mount /mnt/k2hfs\n"
					"     $ umount /mnt/k2hfs\n"
					"     $ fusermount -d /mnt/k2hfs\n"
					"\n"
					" * k2hftfuse options:\n"
					"     -h(--help)                   print help\n"
					"     -v(--version)                print version\n"
					"     -d(--debug)                  set debug level \"msg\", this option is common with FUSE\n"
					"     -o umask=<number>            set umask with FUSE\n"
					"     -o uid=<number>              set uid with FUSE\n"
					"     -o gid=<number>              set gid with FUSE\n"
					"     -o dbglevel={err|wan|msg}    set debug level affect only k2hftfuse\n"
					"     -o conf=<configration file>  specify configration file(.ini .yaml .json) for k2hftfuse and all sub system\n"
					"     -o json=<json string>        specify json string as configration for k2hftfuse and all sub system\n"
					"     -o enable_run_chmpx          run chmpx slave process\n"
					"     -o disable_run_chmpx         do not run chmpx slave process(default)\n"
					"     -o chmpxlog=<log file path>  chmpx log file path when k2hftfuse run chmpx\n"
					"\n"
					" * k2hftfuse environments:\n"
					"     K2HFTCONFFILE                specify configration file(.ini .yaml .json) instead of conf option.\n"
					"     K2HFTJSONCONF                specify json string as configration instead of json option.\n"
					"\n"
					" Please see man page - k2hftfuse(1) for more details.\n"
					"\n"	);
}

//---------------------------------------------------------
// Utility
//---------------------------------------------------------
inline static struct fuse_context* get_and_check_context(void)
{
	struct fuse_context*	pcxt = fuse_get_context();
	assert(pcxt);
	return pcxt;
}

inline static K2hFtManage* get_k2hftman_from_context(struct fuse_context* pcxt = NULL)
{
	if(!pcxt){
		pcxt = fuse_get_context();
	}
	assert(pcxt && pcxt->private_data);
	return reinterpret_cast<K2hFtManage*>(pcxt->private_data);
}

inline static bool check_permission_raw(int mask, const struct stat& stbuf, uid_t exec_uid, gid_t exec_gid)
{
	// like access() function.
	//
	if(R_OK == mask){
		if(	(0 == exec_uid)																					||	// root user(always allowed)
			(S_IRUSR == (stbuf.st_mode & S_IRUSR) && exec_uid == stbuf.st_uid)								||	// same uid
			(S_IRGRP == (stbuf.st_mode & S_IRGRP) && is_gid_include_ids(stbuf.st_gid, exec_uid, exec_gid))	||	// same gid or uid included file's gid
			(S_IROTH == (stbuf.st_mode & S_IROTH))															)	// allowed other
		{
			return true;
		}
	}else if(W_OK == mask){
		if(	(0 == exec_uid)																					||	// root user(always allowed)
			(S_IWUSR == (stbuf.st_mode & S_IWUSR) && exec_uid == stbuf.st_uid)								||	// same uid
			(S_IWGRP == (stbuf.st_mode & S_IWGRP) && is_gid_include_ids(stbuf.st_gid, exec_uid, exec_gid))	||	// same gid or uid included file's gid
			(S_IWOTH == (stbuf.st_mode & S_IWOTH))															)	// allowed other
		{
			return true;
		}
	}else if(X_OK == mask){
		if(	(S_IXUSR == (stbuf.st_mode & S_IXUSR) && 0 == exec_uid)											||	// root user(owner has x_ok, allowed)
			(S_IXUSR == (stbuf.st_mode & S_IXUSR) && exec_uid == stbuf.st_uid)								||	// same uid
			(S_IXGRP == (stbuf.st_mode & S_IXGRP) && is_gid_include_ids(stbuf.st_gid, exec_uid, exec_gid))	||	// same gid or uid included file's gid
			(S_IXOTH == (stbuf.st_mode & S_IXOTH))															)	// allowed other
		{
			return true;
		}
	}
	return false;
}

inline static int check_permission_access(int mask, const struct stat& stbuf, uid_t exec_uid, gid_t exec_gid)
{
	if(F_OK == mask){
		return 0;
	}
	if(R_OK == (mask & R_OK)){
		if(!check_permission_raw(R_OK, stbuf, exec_uid, exec_gid)){
			return -EACCES;
		}
	}
	if(W_OK == (mask & W_OK)){
		if(!check_permission_raw(W_OK, stbuf, exec_uid, exec_gid)){
			return -EACCES;
		}
	}
	if(X_OK == (mask & X_OK)){
		if(!check_permission_raw(X_OK, stbuf, exec_uid, exec_gid)){
			return -EPERM;
		}
	}
	return 0;
}

inline static bool check_premission_owner(const struct stat& stbuf, uid_t exec_uid)
{
	if(0 == exec_uid){
		return true;		// always root is allowed.
	}
	if(exec_uid == stbuf.st_uid){
		return true;
	}
	return false;
}

//---------------------------------------------------------
// Signals
//---------------------------------------------------------
// [NOTE]
// About terminating k2hftfuse.
//
// k2hftfuse supports executing plugin programs which are managed by k2hftfuse.
// The plugin's standard output(stdout) is redirected any file by configuration.
// If you set plugin's stdout to a file under k2hftfuse mount point, you can
// run filter(plugin) in a multistage.
// 
// As the cause of this redirect stdout function, you can not stop(umount)
// k2hftfuse normally with fusermount etc.
// Then fusermount will return fault with "device busy"(disk sleep) message.
// 
// To shutdown k2hftfuse normally, you will have to do following:
//  1) run fusermount with -l(lazy) option.
//  2) kill(stop) all plugins which is executed by k2hftfuse at same time.
// It is difficult to perform this technique.
// 
// Therefore, we have prepared a procedure for terminating k2hftfuse successfully.
// It is that you send signal HUP/TERM/INT to k2hftfuse.
// k2hftfuse signal handler stops all plugins and runs fusermount.
// It result in you can terminate k2hftfuse as normal.
// 
static bool set_signal_handler(int sig, void (*handler)(int))
{
	// set signal handler
	struct sigaction	sa;
	sigemptyset(&sa.sa_mask);
	sigaddset(&sa.sa_mask, sig);
	sa.sa_flags		= 0;
	sa.sa_handler	= handler;
	if(-1 == sigaction(sig, &sa, NULL)){
		ERR_K2HFTPRN("Could not set signal %d handler. errno = %d", sig, errno);
		return false;
	}
	return true;
}

static void exit_signal_handler(int sig)
{
	assert(SIGHUP == sig || SIGTERM == sig || SIGINT == sig);

	// stop all plugins
	if(pK2hFtMan){
		pK2hFtMan->Clean();
	}

	// umount by fusermount
	int		rtncode;
	string	exitcmd(K2HFT_FUSERMOUNT_CMDLINE_PREFIX);
	exitcmd += GetMountPoint();
	exitcmd += K2HFT_FUSERMOUNT_CMDLINE_SUFIX;
	if(0 != (rtncode = system(exitcmd.c_str()))){
		ERR_K2HFTPRN("could not run fusermount command. return code(%d)", rtncode);
	}
}

//---------------------------------------------------------
// Fuse hook functions
//---------------------------------------------------------
static int k2hft_opt_proc(void* data, const char* arg, int key, struct fuse_args* outargs)
{
	if(FUSE_OPT_KEY_NONOPT == key){
		// This is mount point
		if(!GetMountPoint().empty()){
			K2HFTERR("Unknown option: %s", arg);
			return -1;
		}
		// check mount point
		if(!check_path_directory_type(arg)){
			K2HFTERR("The mount point option(%s) is somthing wrong.", arg);
			return -1;
		}
		// check mount point attributes
		if(!check_mountpoint_attr(arg)){
			K2HFTERR("Does not allow to access mount point(%s).", arg);
			return -1;
		}
		GetMountPoint() = arg;

	}else if(FUSE_OPT_KEY_OPT == key){
		// k2hft options
		if(0 == strcasecmp(arg, "-d") || 0 == strcasecmp(arg, "--debug")){
			// if specified debug option for fuse, k2hft sets debug level as MSG too.
			SetK2hFtDbgMode(K2HFTDBG_MSG);

		}else if(0 == strcasecmp(arg, "-h") || 0 == strcasecmp(arg, "--help")){
			// help mode
			help_mode = true;
			return 0;	// stop extraditing

		}else if(0 == strcasecmp(arg, "-v") || 0 == strcasecmp(arg, "--version")){
			// help mode
			version_mode = true;
			return 0;	// stop extraditing

		}else if(0 == strcasecmp(arg, "-f")){
			// foreground mode
			k2hft_foreground_mode = true;

		}else if(0 == strncasecmp(arg, "umask=", strlen("umask="))){
			// -o umask=XXX
			const char*	pparam = strchr(arg, '=') + sizeof(char);
			if(!cvt_mode_string(pparam, opt_umask)){
				K2HFTERR("Option umask has wrong paramter(%s)", pparam);
				return -1;
			}
			// set umask
			umask(opt_umask);
			is_set_umask = true;

		}else if(0 == strncasecmp(arg, "uid=", strlen("uid="))){
			// -o uid=XXX
			const char*	pparam = strchr(arg, '=') + sizeof(char);
			opt_uid		= static_cast<uid_t>(atoi(pparam));
			is_set_uid	= true;

		}else if(0 == strncasecmp(arg, "gid=", strlen("gid="))){
			// -o gid=XXX
			const char*	pparam = strchr(arg, '=') + sizeof(char);
			opt_gid		= static_cast<gid_t>(atoi(pparam));
			is_set_gid	= true;

		}else if(0 == strncasecmp(arg, "dbglevel=", strlen("dbglevel="))){
			// -o dbglevel=XXX
			const char*	pparam = strchr(arg, '=') + sizeof(char);
			if(!SetK2hFtDbgModeByString(pparam)){
				K2HFTERR("Option dbglevel has wrong paramter(%s)", pparam);
				return -1;
			}
			return 0;	// stop extraditing

		}else if(0 == strncasecmp(arg, "conf=", strlen("conf="))){
			// -o conf=XXX
			if(!GetConfig().empty()){
				K2HFTERR("Option conf is specified, but already specify json option.");
				return -1;
			}
			const char*	pparam = strchr(arg, '=') + sizeof(char);
			if(!check_path_real_path(pparam, GetConfig())){
				K2HFTERR("Option conf has wrong paramter(%s)", pparam);
				return -1;
			}
			return 0;	// stop extraditing

		}else if(0 == strncasecmp(arg, "json=", strlen("json="))){
			// -o json=XXX
			if(!GetConfig().empty()){
				K2HFTERR("Option json is specified, but already specify conf option.");
				return -1;
			}
			const char*	pparam = strchr(arg, '=') + sizeof(char);
			GetConfig() = pparam;
			return 0;	// stop extraditing

		}else if(0 == strcasecmp(arg, "disable_run_chmpx")){
			// -o disable_run_chmpx
			is_run_chmpx = false;
			return 0;	// stop extraditing

		}else if(0 == strcasecmp(arg, "enable_run_chmpx")){
			// -o enable_run_chmpx
			is_run_chmpx = true;
			return 0;	// stop extraditing

		}else if(0 == strncasecmp(arg, "chmpxlog=", strlen("chmpxlog="))){
			// -o conf=XXX
			const char*	pparam = strchr(arg, '=') + sizeof(char);
			GetChmpxLog() = pparam;
			return 0;	// stop extraditing
		}
	}
	return 1;	// extradite to fuse parser
}

//---------------------------------------------------------
// Hook FUSE callbacks
//---------------------------------------------------------
static int h2htpfs_getattr(const char* path, struct stat* stbuf);
static int h2htpfs_readlink(const char* path, char* buf, size_t size);
static int h2htpfs_mknod(const char* path, mode_t mode, dev_t dev);
static int h2htpfs_mkdir(const char* path, mode_t mode);
static int h2htpfs_unlink(const char* path);
static int h2htpfs_rmdir(const char* path);
static int h2htpfs_symlink(const char* target, const char* linkpath);
static int h2htpfs_rename(const char* oldpath, const char* newpath);
static int h2htpfs_link(const char* oldpath, const char* newpath);
static int h2htpfs_chmod(const char* path, mode_t mode);
static int h2htpfs_chown(const char* path, uid_t uid, gid_t gid);
static int h2htpfs_truncate(const char* path, off_t length);
static int h2htpfs_open(const char* path, struct fuse_file_info* fi);
static int h2htpfs_read(const char* path, char* buf, size_t size, off_t offset, struct fuse_file_info* fi);
static int h2htpfs_write(const char* path, const char* buf, size_t size, off_t offset, struct fuse_file_info* fi);
static int h2htpfs_statfs(const char* path, struct statvfs* stbuf);
static int h2htpfs_flush(const char* path, struct fuse_file_info* fi);
static int h2htpfs_release(const char* path, struct fuse_file_info* fi);
static int h2htpfs_fsync(const char* path, int isdatasync, struct fuse_file_info* fi);
static int h2htpfs_opendir(const char* path, struct fuse_file_info* fi);
static int h2htpfs_readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi);
static void* h2htpfs_init(struct fuse_conn_info *conn);
static void h2htpfs_destroy(void* data);
static int h2htpfs_access(const char* path, int mask);
static int h2htpfs_create(const char* path, mode_t mode, struct fuse_file_info* fi);
static int h2htpfs_utimens(const char* path, const struct timespec ts[2]);

//
// Hooks
//
static int h2htpfs_getattr(const char* path, struct stat* stbuf)
{
	//
	// Do not handle fgetattr callback, thus call this function.
	//
	memset(stbuf, 0, sizeof(struct stat));		// st_rdev and st_dev is not set because these value is ignored by fuse.

	K2hFtManage*	pk2hftman = get_k2hftman_from_context();
	if(!pk2hftman->FindPath(path, *stbuf)){
		MSG_K2HFTPRN("Could not find %s path for getting stat.", path);
		return -ENOENT;
	}
	return 0;
}

static int h2htpfs_readlink(const char* path, char* buf, size_t size)
{
	return -EPERM;
}

static int h2htpfs_mknod(const char* path, mode_t mode, dev_t dev)
{
	return -EPERM;
}

static int h2htpfs_mkdir(const char* path, mode_t mode)
{
	return -EPERM;
}

static int h2htpfs_unlink(const char* path)
{
	return -EPERM;
}

static int h2htpfs_rmdir(const char* path)
{
	return -EPERM;
}

static int h2htpfs_symlink(const char* target, const char* linkpath)
{
	return -EPERM;
}

static int h2htpfs_rename(const char* oldpath, const char* newpath)
{
	return -EPERM;
}

static int h2htpfs_link(const char* oldpath, const char* newpath)
{
	return -EPERM;
}

static int h2htpfs_chmod(const char* path, mode_t mode)
{
	// [NOTE]
	// k2hftfuse does not allow to change directory mode
	//

	// check mount point
	if(0 == strcmp(path, "/")){
		MSG_K2HFTPRN("path is mount point, so could not change mode.");
		return -EPERM;
	}

	// get context
	struct fuse_context* 	pfusectx	= get_and_check_context();
	K2hFtManage*			pk2hftman	= get_k2hftman_from_context(pfusectx);

	// check owner
	struct stat	stbuf;
	if(!pk2hftman->FindPath(path, stbuf)){
		MSG_K2HFTPRN("Could not find %s path for checking access.", path);
		return -ENOENT;
	}
	if(!check_premission_owner(stbuf, pfusectx->uid)){
		MSG_K2HFTPRN("uid(%d) is not path(%s) owner.", pfusectx->uid, path);
		return -EPERM;
	}
	// if target path is directory, error.
	if(!S_ISREG(stbuf.st_mode)){
		ERR_K2HFTPRN("Not support to change mode for directory.");
		return -EPERM;
	}

	// check parent dir access
	string	parentpath;
	if(!get_parent_dirpath(path, parentpath)){
		ERR_K2HFTPRN("Could not get parent directory path from path(%s).", path);
		return -EACCES;
	}
	struct stat	parent_stbuf;
	if(!pk2hftman->FindPath(parentpath.c_str(), parent_stbuf)){
		MSG_K2HFTPRN("Could not find %s path for checking access.", parentpath.c_str());
		return -ENOENT;
	}
	int	result;
	if(0 != (result = check_permission_access(X_OK, parent_stbuf, pfusectx->uid, pfusectx->gid))){
		ERR_K2HFTPRN("parent directory path(%s) does not allow to change.", path);
		return result;
	}

	// check mode
	mode &= (S_IRWXU | S_IRWXG | S_IRWXO);			// can change only accessing bit.

	// set mode
	if(!pk2hftman->SetMode(path, mode)){
		MSG_K2HFTPRN("Could not change file(%s) mode(%04o).", path, mode);
		return -EIO;
	}
	return 0;
}

static int h2htpfs_chown(const char* path, uid_t uid, gid_t gid)
{
	// [NOTE]
	// k2hftfuse does not allow to change directory owner and group
	//

	// check mount point
	if(0 == strcmp(path, "/")){
		MSG_K2HFTPRN("path is mount point, so could not change owner/group.");
		return -EPERM;
	}

	// get context
	struct fuse_context* 	pfusectx	= get_and_check_context();
	K2hFtManage*			pk2hftman	= get_k2hftman_from_context(pfusectx);

	// check owner
	struct stat	stbuf;
	if(!pk2hftman->FindPath(path, stbuf)){
		MSG_K2HFTPRN("Could not find %s path for checking access.", path);
		return -ENOENT;
	}
	if(!check_premission_owner(stbuf, pfusectx->uid)){
		MSG_K2HFTPRN("uid(%d) is not path(%s) owner.", pfusectx->uid, path);
		return -EPERM;
	}
	// if target path is directory, error.
	if(!S_ISREG(stbuf.st_mode)){
		ERR_K2HFTPRN("Allow to change owner/group only regular file.");
		return -EPERM;
	}

	// check parent dir access
	string	parentpath;
	if(!get_parent_dirpath(path, parentpath)){
		ERR_K2HFTPRN("Could not get parent directory path from path(%s).", path);
		return -EACCES;
	}
	struct stat	parent_stbuf;
	if(!pk2hftman->FindPath(parentpath.c_str(), parent_stbuf)){
		MSG_K2HFTPRN("Could not find %s path for checking access.", parentpath.c_str());
		return -ENOENT;
	}
	int	result;
	if(0 != (result = check_permission_access(X_OK, parent_stbuf, pfusectx->uid, pfusectx->gid))){
		ERR_K2HFTPRN("parent directory path(%s) does not allow.", path);
		return result;
	}

	// check uid/gid
	if(uid != static_cast<uid_t>(-1)){
		struct passwd*	pwdata = getpwuid(uid);
		if(pwdata){
			uid = pwdata->pw_uid;
		}
	}
	if(gid != static_cast<gid_t>(-1)){
		struct group*	grdata = getgrgid(gid);
		if(grdata){
			gid = grdata->gr_gid;
		}
	}

	// set uid/gid
	if(!pk2hftman->SetOwner(path, uid, gid)){
		MSG_K2HFTPRN("Could not change file(%s) owner(%d)/group(%d).", path, uid, gid);
		return -EIO;
	}
	return 0;
}

static int h2htpfs_truncate(const char* path, off_t length)
{
	//
	// Do not handle ftruncate callback, thus call this function.
	//
	if(0 != length){
		MSG_K2HFTPRN("truncate must be length = 0, request %zd byte.", length);
		return -EPERM;
	}

	K2hFtManage*	pk2hftman = get_k2hftman_from_context();
	if(!pk2hftman->TruncateZero(path)){
		MSG_K2HFTPRN("Could not change file(%s) size to %zd(must be zero).", path, length);
		return -EIO;
	}
	return 0;
}

static int h2htpfs_open(const char* path, struct fuse_file_info* fi)
{
	// [NOTE]
	// fi->fh is PK2HFTRULE pointer for direct access. but must access through K2hFtInfo class method.
	//

	// check open mode
	if(O_RDONLY == (fi->flags & O_ACCMODE)){
		MSG_K2HFTPRN("file(%s) open read only mode, but does not reply error here.", path);
		//return -EACCES;
	}

	// get context
	struct fuse_context* 	pfusectx	= get_and_check_context();
	K2hFtManage*			pk2hftman	= get_k2hftman_from_context(pfusectx);

	// check path directory?
	struct stat	stbuf;
	uint64_t	file_handle= 0;
	if(pk2hftman->FindPath(path, stbuf)){
		if(S_ISDIR(stbuf.st_mode)){
			ERR_K2HFTPRN("path(%s) is directory.", path);
			return -EISDIR;
		}
		file_handle = pk2hftman->GetFileHandle(path);
	}

	// check parent dir access
	string	parentpath;
	if(!get_parent_dirpath(path, parentpath)){
		ERR_K2HFTPRN("Could not get parent directory path from path(%s).", path);
		return -EACCES;
	}
	struct stat	parent_stbuf;
	if(!pk2hftman->FindPath(parentpath.c_str(), parent_stbuf)){
		MSG_K2HFTPRN("Could not find %s path for checking access.", parentpath.c_str());
		return -ENOENT;
	}
	int	result;
	if(0 != (result = check_permission_access(X_OK, parent_stbuf, pfusectx->uid, pfusectx->gid))){
		ERR_K2HFTPRN("parent directory path(%s) does not allow to execute.", path);
		return result;
	}

	if(0 == file_handle){
		// create file object in K2hFtInfo
		if(0 != (result = check_permission_access(W_OK, parent_stbuf, pfusectx->uid, pfusectx->gid))){
			ERR_K2HFTPRN("parent directory path(%s) does not allow to write.", path);
			return result;
		}

		mode_t	mode = ((S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH) & ~(pfusectx->umask)) | S_IFREG;
		if(!pk2hftman->CreateFile(path, mode, pfusectx->uid, pfusectx->gid, file_handle)){
			ERR_K2HFTPRN("could not create path(%s) file.", path);
			return -EIO;
		}
	}
	// set handle
	fi->fh = file_handle;

	return 0;
}

static int h2htpfs_read(const char* path, char* buf, size_t size, off_t offset, struct fuse_file_info* fi)
{
	// [NOTE]
	// k2hftfuse support only writable file system.
	// We can make it by permission, but we make it by this function.
	// This returns always "Input output error"(EIO), so it works fine for seem 
	// not readable file system.
	//
	return -EIO;
}

static int h2htpfs_write(const char* path, const char* buf, size_t size, off_t offset, struct fuse_file_info* fi)
{
	// [NOTE]
	// offset is not used.
	//
	if(0 == fi->fh){
		ERR_K2HFTPRN("file handle is null for path(%s).", path);
		return -EIO;
	}

	// get context
	struct fuse_context* 	pfusectx	= get_and_check_context();
	K2hFtManage*			pk2hftman	= get_k2hftman_from_context(pfusectx);
	if(!pk2hftman->Push(reinterpret_cast<const unsigned char*>(buf), size, fi->fh, pfusectx->pid)){
		ERR_K2HFTPRN("Failed to write data(length=%zu) to path(%s)", size, path);
		return -EIO;
	}
	return static_cast<int>(size);
}

static int h2htpfs_statfs(const char* path, struct statvfs* stbuf)
{
	// Set values as 1TB area with no files.
	stbuf->f_bsize		= get_system_pagesize();							// == 4KB
	stbuf->f_frsize		= stbuf->f_bsize;
	stbuf->f_blocks		= (1024 * 1024 * 1024) / (stbuf->f_bsize / 1024);	// 1TB
	stbuf->f_bfree		= stbuf->f_blocks;
	stbuf->f_bavail		= stbuf->f_blocks;
	stbuf->f_files		= stbuf->f_blocks / 4;								// inode = 1/4 blocks
	stbuf->f_ffree		= stbuf->f_files;
	stbuf->f_favail		= stbuf->f_files;
	stbuf->f_namemax	= NAME_MAX;

	return 0;
}

static int h2htpfs_flush(const char* path, struct fuse_file_info* fi)
{
	// [NOTE]
	// This method is called at close and flush. we handles this on binary mode
	//
	if(0 == fi->fh){
		return 0;
	}
	// get context
	struct fuse_context* 	pfusectx	= get_and_check_context();
	K2hFtManage*			pk2hftman	= get_k2hftman_from_context(pfusectx);
	if(!pk2hftman->IsBinMode()){
		return 0;
	}
	// binary mode
	if(!pk2hftman->Flush(fi->fh, pfusectx->pid)){
		ERR_K2HFTPRN("Failed to flush for path(%s), but returns no error.", path);
	}
	return 0;
}

static int h2htpfs_release(const char* path, struct fuse_file_info* fi)
{
	// [NOTE]
	// This method is called lastest closing file, so we need to flush stack.
	//
	if(0 == fi->fh){
		return 0;
	}

	// get context
	struct fuse_context* 	pfusectx	= get_and_check_context();
	K2hFtManage*			pk2hftman	= get_k2hftman_from_context(pfusectx);
	if(!pk2hftman->Close(fi->fh, pfusectx->pid)){
		ERR_K2HFTPRN("Failed to close(with data sync) for path(%s), but returns no error.", path);
	}
	return 0;
}

static int h2htpfs_fsync(const char* path, int isdatasync, struct fuse_file_info* fi)
{
	// [NOTE]
	// This function only works update mtime, not flush stack data.
	//
	if(isdatasync){
		// If datasync is not 0, not need to update mtime.
		return 0;
	}
	if(0 == fi->fh){
		return 0;
	}

	// get context
	K2hFtManage*	pk2hftman = get_k2hftman_from_context();
	if(!pk2hftman->UpdateMTime(fi->fh)){
		ERR_K2HFTPRN("Failed to update mtime for path(%s)", path);
		return -EIO;
	}
	return 0;
}

static int h2htpfs_opendir(const char* path, struct fuse_file_info* fi)
{
	// All objects(directories and files) have same uid, gid and mode.
	// Because those are made by this process or clients. The file which was made by clients was allowed
	// to access(create)ed by this process. It means the accessing result was allowed by comparing this
	// process's uid/gid/mode.
	// Thus this process has all objects is allowed if it is existed.
	//
	return 0;
}

static int h2htpfs_readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi)
{
	// [NOTE]
	// offset anf fi are not used in this function.
	//
	K2hFtManage*	pk2hftman	= get_k2hftman_from_context();
	k2hftldlist_t	list;
	if(!pk2hftman->ReadDir(path, list)){
		MSG_K2HFTPRN("Could not listing file/directory under %s directory.", path);
		free_k2hftldlist(list);
		return -EIO;
	}

	// fill
	for(k2hftldlist_t::iterator iter = list.begin(); iter != list.end(); ++iter){
		if(1 == filler(buf, (*iter)->strname.c_str(), (*iter)->pstat, 0)){
			WAN_K2HFTPRN("Something wrong in filler for %s in %s, but continue...", (*iter)->strname.c_str(), path);
		}
	}
	free_k2hftldlist(list);

	return 0;
}

static void* h2htpfs_init(struct fuse_conn_info *conn)
{
	// put version into syslog
	FORCE_K2HFTPRN("run h2kfuse version %s(%s) with %s", VERSION, k2hftfuse_commit_hash, K2H_TRANS_VER_FUNC());

	// return pK2hFtMan as private data.
	assert(NULL != pK2hFtMan);

	// start to run all plugin
	if(pK2hFtMan && !pK2hFtMan->ExecPlugins()){
		ERR_K2HFTPRN("Failed to start(execute) all plugin, but continue...");
	}

	return pK2hFtMan;
}

static void h2htpfs_destroy(void* data)
{
	K2hFtManage*	pPrivateData = reinterpret_cast<K2hFtManage*>(data);
	assert(pPrivateData == pK2hFtMan);

	if(pK2hFtMan){
		pK2hFtMan->Clean();
	}
}

static int h2htpfs_access(const char* path, int mask)
{
	struct fuse_context* 	pfusectx	= get_and_check_context();
	K2hFtManage*			pk2hftman	= get_k2hftman_from_context(pfusectx);
	struct stat				stbuf;
	if(!pk2hftman->FindPath(path, stbuf)){
		MSG_K2HFTPRN("Could not find %s path for checking access.", path);
		return -ENOENT;
	}

	return check_permission_access(mask, stbuf, pfusectx->uid, pfusectx->gid);
}

static int h2htpfs_create(const char* path, mode_t mode, struct fuse_file_info* fi)
{
	// [NOTE]
	// fi->fh is PK2HFTRULE pointer for direct access. but must access through K2hFtInfo class method.
	//

	// get context
	struct fuse_context* 	pfusectx	= get_and_check_context();
	K2hFtManage*			pk2hftman	= get_k2hftman_from_context(pfusectx);

	// does file exist?
	struct stat	stbuf;
	if(pk2hftman->FindPath(path, stbuf)){
		if(S_ISDIR(stbuf.st_mode)){
			ERR_K2HFTPRN("path(%s) is directory.", path);
			return -EISDIR;
		}
		ERR_K2HFTPRN("path(%s) already exists.", path);
		return -EEXIST;
	}

	// check parent dir access
	string	parentpath;
	if(!get_parent_dirpath(path, parentpath)){
		ERR_K2HFTPRN("Could not get parent directory path from path(%s).", path);
		return -EACCES;
	}
	struct stat	parent_stbuf;
	if(!pk2hftman->FindPath(parentpath.c_str(), parent_stbuf)){
		MSG_K2HFTPRN("Could not find %s path for checking access.", parentpath.c_str());
		return -ENOENT;
	}
	int	result;
	if(0 != (result = check_permission_access(X_OK, parent_stbuf, pfusectx->uid, pfusectx->gid))){
		ERR_K2HFTPRN("parent directory path(%s) does not allow to execute.", path);
		return result;
	}
	if(0 != (result = check_permission_access(W_OK, parent_stbuf, pfusectx->uid, pfusectx->gid))){
		ERR_K2HFTPRN("parent directory path(%s) does not allow to write.", path);
		return result;
	}

	// create file object in K2hFtInfo
	if(!pk2hftman->CreateFile(path, mode, pfusectx->uid, pfusectx->gid, fi->fh)){
		ERR_K2HFTPRN("could not create path(%s) file.", path);
		return -EIO;
	}
	return 0;
}

static int h2htpfs_utimens(const char* path, const struct timespec ts[2])
{
	//
	// Do not handle utime callback, thus call this function.
	//

	// [NOTE]
	// k2hftfuse does not allow to change directory mtime(atime)
	// Files has mtime and atime, but both times are same.
	// So this function always uses ts[1] for mtime, and set it to mtime and atime.
	//

	// check mount point
	if(0 == strcmp(path, "/")){
		MSG_K2HFTPRN("path is mount point, so could not change mtime(atime).");
		return -EPERM;
	}

	// get context
	struct fuse_context* 	pfusectx	= get_and_check_context();
	K2hFtManage*			pk2hftman	= get_k2hftman_from_context(pfusectx);

	// check owner
	struct stat	stbuf;
	if(!pk2hftman->FindPath(path, stbuf)){
		MSG_K2HFTPRN("Could not find %s path for checking access.", path);
		return -ENOENT;
	}
	if(!check_premission_owner(stbuf, pfusectx->uid)){
		MSG_K2HFTPRN("uid(%d) is not path(%s) owner.", pfusectx->uid, path);
		return -EPERM;
	}
	// if target path is directory, error.
	if(!S_ISREG(stbuf.st_mode)){
		ERR_K2HFTPRN("Allow to change mtime(atime) only regular file.");
		return -EPERM;
	}

	// check parent dir access
	string	parentpath;
	if(!get_parent_dirpath(path, parentpath)){
		ERR_K2HFTPRN("Could not get parent directory path from path(%s).", path);
		return -EACCES;
	}
	struct stat	parent_stbuf;
	if(!pk2hftman->FindPath(parentpath.c_str(), parent_stbuf)){
		MSG_K2HFTPRN("Could not find %s path for checking access.", parentpath.c_str());
		return -ENOENT;
	}
	int	result;
	if(0 != (result = check_permission_access(X_OK, parent_stbuf, pfusectx->uid, pfusectx->gid))){
		ERR_K2HFTPRN("parent directory path(%s) does not allow.", path);
		return result;
	}

	// set timespec(mtime)
	if(!pk2hftman->SetTimespec(path, ts[1])){
		MSG_K2HFTPRN("Could not change file(%s) mtime(atime).", path);
		return -EIO;
	}
	return 0;
}

//---------------------------------------------------------
// Main
//---------------------------------------------------------
int main(int argc, char** argv)
{
	// set syslog
	InitK2hFtDbgSyslog();

	// parse options
	struct fuse_args	args = FUSE_ARGS_INIT(argc, argv);
	if(0 != fuse_opt_parse(&args, NULL, NULL, k2hft_opt_proc)){
		exit(EXIT_FAILURE);
	}

	// check options
	if(version_mode){
		k2hftfuse_print_version(stdout);
		exit(EXIT_SUCCESS);
	}
	if(help_mode){
		k2hftfuse_print_usage(stdout);
		exit(EXIT_SUCCESS);
	}
	if(GetMountPoint().empty()){
		K2HFTERR("Need to specify mount point path.");
		exit(EXIT_FAILURE);
	}

	// check configuration
	CHMCONFTYPE	conftype = check_chmconf_type_ex(GetConfig().c_str(), K2HFT_CONFFILE_ENV_NAME, K2HFT_JSONCONF_ENV_NAME, &(GetConfig()));
	if(CHMCONF_TYPE_UNKNOWN == conftype || CHMCONF_TYPE_NULL == conftype){
		ERR_K2HPRN("configuration file or json string is wrong.");
		return false;
	}

	// set signal handler
	if(	!set_signal_handler(SIGHUP,	 exit_signal_handler) ||
		!set_signal_handler(SIGTERM, exit_signal_handler) ||
		!set_signal_handler(SIGINT,	 exit_signal_handler) ||
		!set_signal_handler(SIGPIPE, SIG_IGN)			  )
	{
		K2HFTERR("Could not set signal HUP/TERM/INT/PIPE handler.");
		exit(EXIT_FAILURE);
	}

	// initialize all
	pK2hFtMan = new K2hFtManage();
	if(!pK2hFtMan->K2hFtManage::SetMountPoint(GetMountPoint().c_str())){
		K2HFTERR("Something error occurred in initializing internal data about mount point.");
		K2HFT_DELETE(pK2hFtMan);
		exit(EXIT_FAILURE);
	}
	if(!pK2hFtMan->Initialize((is_set_umask ? &opt_umask : NULL), (is_set_uid ? &opt_uid : NULL), (is_set_gid ? &opt_gid : NULL))){
		K2HFTERR("Something error occurred in initializing internal data about uid/gid/mode/etc.");
		K2HFT_DELETE(pK2hFtMan);
		exit(EXIT_FAILURE);
	}
	if(!pK2hFtMan->Initialize(GetConfig().c_str(), is_run_chmpx, GetChmpxLog().c_str())){
		K2HFTERR("Something error occurred in initializing internal data from configration(%s).", GetConfig().c_str());
		K2HFT_DELETE(pK2hFtMan);
		exit(EXIT_FAILURE);
	}

	//
	// init fuse_operations structure
	//
	// [NOTE]
	// for our local environment, not use C99 designated initializers.
	// 
	struct fuse_operations	k2hft_operations;
	{
		memset(&k2hft_operations, 0, sizeof(struct fuse_operations));
		k2hft_operations.getattr			= h2htpfs_getattr;
		k2hft_operations.readlink			= h2htpfs_readlink;
		k2hft_operations.mknod				= h2htpfs_mknod;
		k2hft_operations.mkdir				= h2htpfs_mkdir;
		k2hft_operations.unlink				= h2htpfs_unlink;
		k2hft_operations.rmdir				= h2htpfs_rmdir;
		k2hft_operations.symlink			= h2htpfs_symlink;
		k2hft_operations.rename				= h2htpfs_rename;
		k2hft_operations.link				= h2htpfs_link;
		k2hft_operations.chmod				= h2htpfs_chmod;
		k2hft_operations.chown				= h2htpfs_chown;
		k2hft_operations.truncate			= h2htpfs_truncate;
		k2hft_operations.open				= h2htpfs_open;
		k2hft_operations.read				= h2htpfs_read;
		k2hft_operations.write				= h2htpfs_write;
		k2hft_operations.statfs				= h2htpfs_statfs;
		k2hft_operations.flush				= h2htpfs_flush;
		k2hft_operations.release			= h2htpfs_release;
		k2hft_operations.fsync				= h2htpfs_fsync;
		k2hft_operations.opendir			= h2htpfs_opendir;
		k2hft_operations.readdir			= h2htpfs_readdir;
		k2hft_operations.init				= h2htpfs_init;
		k2hft_operations.destroy			= h2htpfs_destroy;
		k2hft_operations.access				= h2htpfs_access;
		k2hft_operations.create				= h2htpfs_create;
		k2hft_operations.utimens			= h2htpfs_utimens;
	}

	// main fuse loop
	int	exit_code = fuse_main(args.argc, args.argv, &k2hft_operations, NULL);
	fuse_opt_free_args(&args);

	K2HFT_DELETE(pK2hFtMan);

	exit(exit_code);
}

/*
 * VIM modelines
 *
 * vim:set ts=4 fenc=utf-8:
 */
