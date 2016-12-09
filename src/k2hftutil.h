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

#ifndef	K2HFTUTIL_H
#define K2HFTUTIL_H

#include <sys/types.h>
#include <string>
#include <sstream>
#include <list>

#include <fullock/flckutil.h>		// for string utility

//---------------------------------------------------------
// Typedefs
//---------------------------------------------------------
typedef std::list<std::string>			strlst_t;

//---------------------------------------------------------
// Variables
//---------------------------------------------------------
extern const struct timespec			common_init_time;

//---------------------------------------------------------
// Macros
//---------------------------------------------------------
#define	GET_STAT_BLOCKS(size)			(((size) / 512) + ((0 != ((size) % 512)) ? 1 : 0))

//---------------------------------------------------------
// Utilities
//---------------------------------------------------------
bool get_name_by_uid(uid_t uid, std::string& name);
bool is_gid_include_ids(gid_t tggid, uid_t uid, gid_t gid);

bool check_path_directory_type(const char* path);
bool check_mountpoint_attr(const char* path);
bool check_path_real_path(const char* path, std::string& abspath);
bool mkdir_by_abs_path(const char* path, std::string& abspath);
bool make_file_by_abs_path(const char* path, mode_t mode, std::string& abspath, bool is_make_path = false);
bool is_file_charactor_device(const char* path);
bool remove_exist_file(const char* path);

size_t get_system_pagesize(void);

bool cvt_mode_string(const char* strmode, mode_t& mode);
bool set_now_timespec(struct timespec& stime, bool is_coarse = true);

bool get_parent_dirpath(const char* path, std::string& parent);
bool get_file_name(const char* path, std::string& fname);
bool k2hft_write(int fd, const unsigned char* data, size_t length);

const char* get_local_hostname(void);
void get_local_hostname(char* phostname);

int convert_string_to_strlst(const char* base, strlst_t& list);
char** convert_strlst_to_chararray(const strlst_t& list);
void free_chararray(char** pparray);

#endif	// K2HFTUTIL_H

/*
 * VIM modelines
 *
 * vim:set ts=4 fenc=utf-8:
 */
