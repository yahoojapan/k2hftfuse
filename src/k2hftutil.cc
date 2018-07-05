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
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <pwd.h>
#include <grp.h>

#include <string>

#include "k2hftcommon.h"
#include "k2hftutil.h"
#include "k2hftdbg.h"

using namespace std;

//---------------------------------------------------------
// Variables
//---------------------------------------------------------
const struct timespec	common_init_time = {0, 0};	// {.tv_sec = 0, .tv_nsec = 0};

//---------------------------------------------------------
// Utilities
//---------------------------------------------------------
bool get_name_by_uid(uid_t uid, string& name)
{
	// for allocated buffer
	long	length = sysconf(_SC_GETPW_R_SIZE_MAX);
	if(-1 == length){
		ERR_K2HFTPRN("Could not get _SC_GETPW_R_SIZE_MAX");
		return false;
	}
	char*	ptmp;
	if(NULL == (ptmp = reinterpret_cast<char*>(malloc(sizeof(char) * length)))){
		ERR_K2HFTPRN("Could not allocate memory.");
		return false;
	}

	// get passwd field
	struct passwd	pwd;
	struct passwd*	presult = NULL;
	int				result;
	if(0 != (result = getpwuid_r(uid, &pwd, ptmp, static_cast<size_t>(length), &presult)) || NULL == presult){
		WAN_K2HFTPRN("Could not get passwd field for uid(%d) by errno(%d).", uid, result);
		K2HFT_FREE(ptmp);
		return false;
	}

	name = (presult->pw_name ? presult->pw_name : "");
	K2HFT_FREE(ptmp);
	return true;
}

bool is_gid_include_ids(gid_t tggid, uid_t uid, gid_t gid)
{
	if(tggid == gid){
		return true;
	}

	// get name by uid
	string	name;
	if(!get_name_by_uid(uid, name)){
		WAN_K2HFTPRN("Could not get name by uid(%d).", uid);
		return false;
	}

	// for allocated buffer
	long	length = sysconf(_SC_GETGR_R_SIZE_MAX);
	if(-1 == length){
		ERR_K2HFTPRN("Could not get _SC_GETGR_R_SIZE_MAX");
		return false;
	}
	char*	ptmp;
	if(NULL == (ptmp = reinterpret_cast<char*>(malloc(sizeof(char) * length)))){
		ERR_K2HFTPRN("Could not allocate memory.");
		return false;
	}

	// get group
	struct group	grp;
	struct group*	presult = NULL;
	int				result;
	if(0 != (result = getgrgid_r(tggid, &grp, ptmp, static_cast<size_t>(length), &presult)) || NULL == presult){
		WAN_K2HFTPRN("Could not get uids in group(%d) by errno(%d).", tggid, result);
		K2HFT_FREE(ptmp);
		return false;
	}

	// does include name in name list?
	bool	is_found = false;
	char**	ppmember;
	for(ppmember = presult->gr_mem; ppmember && *ppmember; ppmember++){
		if(name == *ppmember){
			is_found = true;
			break;
		}
	}
	K2HFT_FREE(ptmp);
	return is_found;
}

bool check_path_directory_type(const char* path)
{
	if(K2HFT_ISEMPTYSTR(path)){
		ERR_K2HFTPRN("path is empty.");
		return false;
	}
	if('/' != path[0]){
		ERR_K2HFTPRN("path(%s) must start \"/\" root directory path.", path);
		return false;
	}
	struct stat	st;
	if(-1 == stat(path, &st)){
		ERR_K2HFTPRN("Could not get stat for %s(errno:%d)", path, errno);
		return false;
	}
	if(!S_ISDIR(st.st_mode)){
		MSG_K2HFTPRN("path %s is not directory", path);
		return false;
	}
	return true;
}

//
// must call this function after check_path_directory_type()
//
bool check_mountpoint_attr(const char* path)
{
	struct stat	st;
	if(-1 == stat(path, &st)){
		ERR_K2HFTPRN("Could not get stat for %s(errno:%d)", path, errno);
		return false;
	}

	// check other
	if(S_IRWXO == (st.st_mode & S_IRWXO)){
		return true;
	}

	// check by owner
	uid_t	myuid = geteuid();
	if(0 == myuid || st.st_uid == myuid){
		return true;
	}

	// check by group
	gid_t	mygid = getegid();
	if(is_gid_include_ids(st.st_gid, myuid, mygid)){
		if(S_IRWXG == (st.st_mode & S_IRWXG)){
			return true;
		}
	}
	return false;
}

bool check_path_real_path(const char* path, string& abspath)
{
	if(K2HFT_ISEMPTYSTR(path)){
		ERR_K2HFTPRN("path is empty.");
		return false;
	}
	char*	rpath;
	if(NULL == (rpath = realpath(path, NULL))){
		MSG_K2HFTPRN("Could not convert path(%s) to real path or there is no such file(errno:%d).", path, errno);
		return false;
	}
	abspath = rpath;
	K2HFT_FREE(rpath);
	return true;
}

bool mkdir_by_abs_path(const char* path, string& abspath)
{
	if(K2HFT_ISEMPTYSTR(path)){
		ERR_K2HFTPRN("path is empty.");
		return false;
	}
	if('/' != path[0]){
		ERR_K2HFTPRN("path(%s) must start \"/\" root directory path.", path);
		return false;
	}

	// check
	if(check_path_real_path(path, abspath)){
		if(!check_path_directory_type(abspath.c_str())){
			ERR_K2HFTPRN("path(%s:%s) exists, but it is not directory.", path, abspath.c_str());
			return false;
		}
		return true;
	}
	if(0 == strcmp(path, "/")){
		ERR_K2HFTPRN("path(%s) is \"/\", but something error is caught. why...", path);
		return false;
	}

	// reentrant check for upper directory
	string				tmppath	= path;
	string::size_type	pos		= tmppath.find_last_of('/');
	if(string::npos == pos){
		ERR_K2HFTPRN("Not found \"/\" separator in path(%s).", path);
		return false;
	}
	tmppath	= tmppath.substr(0, pos);
	if(!mkdir_by_abs_path(tmppath.c_str(), abspath)){
		ERR_K2HFTPRN("could not make directory path(%s/..).", path);
		return false;
	}
	abspath = "";

	// make directory
	if(0 != mkdir(path, (S_IRWXU | S_IRWXG | S_IRWXO))){
		ERR_K2HFTPRN("could not make directory path(%s) by error(%d).", path, errno);
		return false;
	}

	// recheck
	if(!check_path_real_path(path, abspath) || !check_path_directory_type(abspath.c_str())){
		ERR_K2HFTPRN("path(%s:%s) exists, but it is not directory.", path, abspath.c_str());
		return false;
	}
	return true;
}

bool make_file_by_abs_path(const char* path, mode_t mode, string& abspath, bool is_make_path)
{
	if(K2HFT_ISEMPTYSTR(path)){
		ERR_K2HFTPRN("path is NULL.");
		return false;
	}
	struct stat	st;
	if(0 == stat(path, &st)){
		// exist
		if(!S_ISREG(st.st_mode) && !S_ISCHR(st.st_mode)){	// allow charactor device(ex. /dev/null)
			ERR_K2HFTPRN("path(%s) already exists, but it is not regular file.", path);
			return false;
		}
	}else{
		// make file
		int	fd;
		if(K2HFT_INVALID_HANDLE == (fd = open(path, O_CREAT | O_RDWR, (mode & (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH))))){
			if(!is_make_path){
				ERR_K2HFTPRN("Could not make file(%s) by errno(%d).", path, errno);
				return false;
			}
			// try to make directory
			string				strtmp	= path;
			string::size_type	pos		= strtmp.find_last_of('/');
			if(string::npos == pos){
				ERR_K2HFTPRN("Could not make file(%s) by errno(%d).", path, errno);
				return false;
			}
			strtmp = strtmp.substr(0, pos);
			if(strtmp.empty()){
				ERR_K2HFTPRN("Could not make file(%s) by errno(%d).", path, errno);
				return false;
			}
			if(!mkdir_by_abs_path(strtmp.c_str(), abspath)){
				ERR_K2HFTPRN("Could not make file(%s) by errno(%d).", path, errno);
				return false;
			}
			// retry to make file
			if(K2HFT_INVALID_HANDLE == (fd = open(path, O_CREAT | O_RDWR, (mode & (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH))))){
				ERR_K2HFTPRN("Could not make file(%s) by errno(%d).", path, errno);
				return false;
			}
		}
		K2HFT_CLOSE(fd);
	}
	return check_path_real_path(path, abspath);
}

bool is_file_charactor_device(const char* path)
{
	if(K2HFT_ISEMPTYSTR(path)){
		ERR_K2HFTPRN("path is NULL.");
		return false;
	}
	struct stat	st;
	if(0 != stat(path, &st)){
		return false;
	}
	// exist
	if(!S_ISCHR(st.st_mode)){
		return false;
	}
	return true;
}

bool remove_exist_file(const char* path)
{
	if(K2HFT_ISEMPTYSTR(path)){
		ERR_K2HFTPRN("path is empty.");
		return false;
	}
	struct stat	st;
	if(-1 == stat(path, &st)){
		//MSG_K2HFTPRN("Could not get stat for %s(errno:%d)", path, errno);
		return true;
	}
	if(!S_ISREG(st.st_mode)){
		ERR_K2HFTPRN("path %s is not regular file.", path);
		return false;
	}
	if(0 != unlink(path)){
		ERR_K2HFTPRN("Could not remove path %s file by errno(%d)", path, errno);
		return false;
	}
	return true;
}

size_t get_system_pagesize(void)
{
	static size_t	pagesize= 4096;	// default for error.
	static bool		is_init	= false;

	if(!is_init){
		long	psize = sysconf(_SC_PAGE_SIZE);
		if(-1 == psize){
			ERR_K2HFTPRN("Could not get page size(errno = %d), so return 4096 byte", errno);
		}else{
			pagesize = static_cast<size_t>(psize);
		}
		is_init = true;
	}
	return pagesize;
}

bool cvt_mode_string(const char* strmode, mode_t& mode)
{
	if(K2HFT_ISEMPTYSTR(strmode)){
		ERR_K2HFTPRN("strmode is empty.");
		return false;
	}
	// skip '0'
	for(; strmode && '\0' != *strmode && '0' == *strmode; ++strmode);

	mode = 0;
	if(0 < strlen(strmode)){
		// for other(lastest word)
		if(strmode[strlen(strmode) - 1] < '0' || '7' < strmode[strlen(strmode) - 1]){
			return false;
		}
		mode += (strmode[strlen(strmode) - 1] - '0');
	}
	if(1 < strlen(strmode)){
		// for group(2'nd word)
		if(strmode[strlen(strmode) - 2] < '0' || '7' < strmode[strlen(strmode) - 2]){
			return false;
		}
		mode += ((strmode[strlen(strmode) - 2] - '0') * 8);
	}
	if(2 < strlen(strmode)){
		// for own(1'st word)
		if(strmode[strlen(strmode) - 3] < '0' || '7' < strmode[strlen(strmode) - 3]){
			return false;
		}
		mode += ((strmode[strlen(strmode) - 3] - '0') * 8 * 8);
	}
	return true;
}

bool set_now_timespec(struct timespec& stime, bool is_coarse)
{
	// CLOCK_REALTIME_COARSE is faster than CLOCK_REALTIME
	//
	if(-1 == clock_gettime((is_coarse ? CLOCK_REALTIME_COARSE : CLOCK_REALTIME), &stime)){
		WAN_K2HFTPRN("Could not get real time by errno(%d)", errno);
		return false;
	}
	return true;
}

bool get_parent_dirpath(const char* path, string& parent)
{
	if(K2HFT_ISEMPTYSTR(path)){
		ERR_K2HFTPRN("path is empty.");
		return false;
	}
	if(0 == strcmp(path, "/")){
		ERR_K2HFTPRN("path(/) is mount point.");
		return false;
	}
	char*	ptmp = strdup(path);
	if('/' == ptmp[strlen(ptmp) - 1]){
		ptmp[strlen(ptmp) - 1] = '\0';
	}
	char*	pos = strrchr(ptmp, '/');
	if(!pos){
		ERR_K2HFTPRN("path(%s) does not have /.", path);
		K2HFT_FREE(ptmp);
		return false;
	}
	*pos = '\0';
	if(0 == strlen(ptmp)){
		// parent is mount point, it is "/".
		parent = "/";
	}else{
		parent = ptmp;
	}
	K2HFT_FREE(ptmp);
	return true;
}

bool get_file_name(const char* path, string& fname)
{
	if(K2HFT_ISEMPTYSTR(path)){
		ERR_K2HFTPRN("path is empty.");
		return false;
	}
	if(0 == strcmp(path, "/")){
		ERR_K2HFTPRN("path(/) is root(or mount point).");
		return false;
	}
	if('/' == path[strlen(path) - 1]){
		fname = "";
		return true;
	}
	char*	pos = strrchr(const_cast<char*>(path), '/');
	if(!pos){
		fname = path;
	}else{
		fname = ++pos;
	}
	return true;
}

bool k2hft_write(int fd, const unsigned char* data, size_t length)
{
	for(ssize_t wrote = 0, onewrote = 0; static_cast<size_t>(wrote) < length; wrote += onewrote){
		if(-1 == (onewrote = write(fd, &data[wrote], (length - wrote)))){
			WAN_K2HFTPRN("Failed to write data(%p) length(%zu) to fd(%d) by errno(%d).", &data[wrote], (length - wrote), fd, errno);
			return false;
		}
	}
	return true;
}

const char* get_local_hostname(void)
{
	static char	s_hostname[HOST_NAME_MAX + 1];
	static bool s_init = false;

	if(!s_init){
		s_init = true;
		memset(s_hostname, 0, HOST_NAME_MAX + 1);
		gethostname(s_hostname, HOST_NAME_MAX + 1);		// no check error
	}
	return s_hostname;
}

void get_local_hostname(char* phostname)
{
	strcpy(phostname, get_local_hostname());
}

//---------------------------------------------------------
// String Array Utilities
//---------------------------------------------------------
#define	SPACE_CAHRS_FOR_PARSER		" \t\r\n"

int convert_string_to_strlst(const char* base, strlst_t& list)
{
	list.clear();
	if(K2HFT_ISEMPTYSTR(base)){
		MSG_K2HFTPRN("base parameter is empty.");
		return 0;
	}

	char*	tmpbase = strdup(base);
	char*	tmpsave = NULL;
	for(char* tmptok = strtok_r(tmpbase, SPACE_CAHRS_FOR_PARSER, &tmpsave); tmptok; tmptok = strtok_r(NULL, SPACE_CAHRS_FOR_PARSER, &tmpsave)){
		if(K2HFT_ISEMPTYSTR(tmptok)){
			// empty string
			continue;
		}
		list.push_back(string(tmptok));
	}
	K2HFT_FREE(tmpbase);

	return list.size();
}

//
// If list is empty list, returns array pointer which has one char*(NULL).
//
char** convert_strlst_to_chararray(const strlst_t& list)
{
	char**	ppstrings;
	if(NULL == (ppstrings = static_cast<char**>(calloc((list.size() + 1), sizeof(char*))))){	// null terminate
		ERR_K2HFTPRN("could not allocate memory.");
		return NULL;
	}
	char**	pptmp = ppstrings;
	for(strlst_t::const_iterator iter = list.begin(); iter != list.end(); ++iter){
		if((*iter).empty()){
			continue;	// why...
		}
		if(NULL == (*pptmp = strdup((*iter).c_str()))){
			ERR_K2HFTPRN("could not allocate memory.");
			continue;	// uh...
		}
		pptmp++;
	}
	return ppstrings;
}

void free_chararray(char** pparray)
{
	for(char** pptmp = pparray; pptmp && *pptmp; ++pptmp){
		K2HFT_FREE(*pptmp);
	}
	K2HFT_FREE(pparray);
}

/*
 * VIM modelines
 *
 * vim:set ts=4 fenc=utf-8:
 */
