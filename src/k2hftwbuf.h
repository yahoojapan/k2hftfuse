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
 * CREATE:   Fri Sep 11 2015
 * REVISION:
 *
 */

#ifndef	K2HFTWBUF_H
#define K2HFTWBUF_H

#include <fullock/flckutil.h>		// for uint64_t

#include <map>

#include "k2hftstructure.h"

//---------------------------------------------------------
// Class K2hFtBinBuff
//---------------------------------------------------------
class K2hFtBinBuff
{
	protected:
		static const size_t	BUFFER_SINGLE_SIZE	= 8192;

		unsigned char*	CacheBuff;
		size_t			CacheBuffSize;
		size_t			CacheBuffPos;

	public:
		K2hFtBinBuff(void);
		virtual ~K2hFtBinBuff(void);

		bool AppendStringBuff(const unsigned char* data, size_t length, bool check_last_char);
		bool TakeoutBuff(unsigned char** data, size_t* length);
		bool IsEmpty(void) const { return (0 == CacheBuffPos); }
		size_t Size(void) const { return CacheBuffPos; }
};

typedef std::map<pid_t, K2hFtBinBuff*>			k2hftbinbufmap_t;

inline void free_k2hftbinbufmap(k2hftbinbufmap_t& mapdata)
{
	for(k2hftbinbufmap_t::iterator iter = mapdata.begin(); iter != mapdata.end(); ++iter){
		K2hFtBinBuff*	pBuff = iter->second;
		K2HFT_DELETE(pBuff);
	}
	mapdata.clear();
}

//---------------------------------------------------------
// Class K2hFtWriteBuff
//---------------------------------------------------------
class K2hFtManage;

class K2hFtWriteBuff
{
	protected:
		static const size_t	DEFAULT_LINE_LIMIT	= 0;	// = no limit
		static const size_t	BUFFER_BINARY_SIZE	= (8192 * 10);
		static size_t		BinaryBuffLimit;
		static size_t		LineByteLimit;
		static size_t		StackLineMax;
		static time_t		StackTimeup;

		const K2hFtManage*	pK2hFtMan;
		uint64_t			filehandle;
		volatile int		inputbuf_lockval;		// like mutex for variables
		volatile int		stack_lockval;			// like mutex for variables
		k2hftbinbufmap_t	InputBuffMap;			// raw binary data map by pid before stacking
		k2hftlinelist_t		Stack;					// stacked binary data, each data has timestamp and hostname in it.

	public:
		static const size_t	DEFAULT_LINE_MAX	= 0;		// assap
		static const time_t	DEFAULT_TIMEUP		= 0;		// assap

	protected:
		K2hFtBinBuff* GetBinBuff(pid_t pid);

		bool StringPush(K2hFtBinBuff* pBuff, const unsigned char* data, size_t length, pid_t pid);
		bool BinaryPush(K2hFtBinBuff* pBuff, const unsigned char* data, size_t length, pid_t pid);
		bool StackPush(unsigned char* data, size_t length, pid_t pid);

	public:
		static void SetBinaryBuffLimit(size_t size) { K2hFtWriteBuff::BinaryBuffLimit = size; }
		static void SetLineByteLimit(size_t size) { K2hFtWriteBuff::LineByteLimit = size; }
		static void SetStackLineMax(size_t linemax) { K2hFtWriteBuff::StackLineMax = linemax; }
		static size_t GetStackLineMax(void) { return K2hFtWriteBuff::StackLineMax; }
		static void SetStackTimeup(time_t timeup) { K2hFtWriteBuff::StackTimeup = timeup; }
		static time_t GetStackTimeup(void) { return K2hFtWriteBuff::StackTimeup; }

		explicit K2hFtWriteBuff(const K2hFtManage* pman = NULL, uint64_t fhandle = 0);
		virtual ~K2hFtWriteBuff(void);

		void Clean(void);
		bool HasStack(void) const { return 0 < Stack.size(); }			// [NOTICE] no locking

		bool Push(const unsigned char* data, size_t length, bool is_binary_mode, pid_t pid);
		bool ForcePush(bool is_binary_mode, pid_t pid) { return Push(NULL, 0, is_binary_mode, pid); }
		bool ForceAllPush(bool is_binary_mode) { return Push(NULL, 0, is_binary_mode, K2HFT_INVALID_PID); }

		PK2HFTVALUE Pop(void);

		bool IsStackLimit(void);
};

// mapping for file handle on fuse(uint64_t) and K2hFtWriteBuff
typedef std::map<uint64_t, K2hFtWriteBuff*>			k2hftwbufmap_t;

inline void free_k2hftwbufmap(k2hftwbufmap_t& mapdata)
{
	for(k2hftwbufmap_t::iterator iter = mapdata.begin(); iter != mapdata.end(); ++iter){
		K2hFtWriteBuff*	pBuff = iter->second;
		if(pBuff){
			pBuff->Clean();
			K2HFT_DELETE(pBuff);
		}
	}
	mapdata.clear();
}

//---------------------------------------------------------
// Utility for PK2HFTLINE
//---------------------------------------------------------
inline void free_k2hftlinelist(k2hftlinelist_t& list)
{
	for(k2hftlinelist_t::iterator iter = list.begin(); iter != list.end(); ++iter){
		PK2HFTLINE	val = *iter;
		FreeK2hFtLine(val);
	}
	list.clear();
}

#endif	// K2HFTWBUF_H

/*
 * VIM modelines
 *
 * vim:set ts=4 fenc=utf-8:
 */
