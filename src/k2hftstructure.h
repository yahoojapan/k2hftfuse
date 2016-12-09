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
 * CREATE:   Mon Sep 14 2015
 * REVISION:
 *
 */

#ifndef	K2HFTSTRUCTURE_H
#define K2HFTSTRUCTURE_H

#include <string.h>
#include <limits.h>
#include <sys/types.h>

#include <vector>

#include "k2hftcommon.h"
#include "k2hftutil.h"
#include "k2hftdbg.h"

//---------------------------------------------------------
// Structure
//---------------------------------------------------------
//
// Structure for One line(data)
//
typedef struct k2hft_line_head{
	size_t				linelength;								// length for data
	struct timespec		ts;										// time stamp
	pid_t				pid;									// pid as client process
	char				hostname[HOST_NAME_MAX + 8];			// force alignment 64bit(we need over HOST_NAME_MAX + 1)
}K2HFT_ATTR_PACKED K2HFTLINEHEAD, *PK2HFTLINEHEAD;

typedef union k2hft_line{
	K2HFTLINEHEAD		head;
	unsigned char		byLine[sizeof(K2HFTLINEHEAD) + 1];
}K2HFT_ATTR_PACKED K2HFTLINE, *PK2HFTLINE;

typedef std::vector<PK2HFTLINE>		k2hftlinelist_t;


//
// Structure for k2hash value which has lines
//
typedef struct k2hft_value_head{
	size_t				bodylength;
	size_t				linecount;								// count of K2HFTLINE
}K2HFT_ATTR_PACKED K2HFTVHEAD, *PK2HFTVHEAD;

typedef union k2hft_value{
	K2HFTVHEAD			head;
	unsigned char		byValue[sizeof(K2HFTVHEAD) + 1];
}K2HFT_ATTR_PACKED K2HFTVALUE, *PK2HFTVALUE;

//---------------------------------------------------------
// Symbols
//---------------------------------------------------------
#define	K2HFT_KEY_SUFFIX_FORM			"%d"
#define	K2HFT_KEY_BUFF_TID_MAXSIZE		32						// enough by 32 charactor string length
#define	K2HFT_LINE_BODY_START_POS		sizeof(K2HFTLINEHEAD)
#define	K2HFT_VALUE_BODY_START_POS		sizeof(K2HFTVHEAD)

//---------------------------------------------------------
// Utility Functions
//---------------------------------------------------------
//
// Key format :	"<file path char array>\0<pid decimal char array>\0"
//				(file path and pid string sepalated by '\0')
//
inline unsigned char* BuildK2hFtKey(size_t& length, const char* path, pid_t pid)
{
	if(!path){
		ERR_K2HFTPRN("path is NULL.");
		return NULL;
	}
	// copy path
	size_t	pathlength = strlen(path);
	char*	pBuff;
	if(NULL == (pBuff = reinterpret_cast<char*>(malloc(pathlength + K2HFT_KEY_BUFF_TID_MAXSIZE)))){
		ERR_K2HFTPRN("Could not allocate memory.");
		return NULL;
	}
	strcpy(pBuff, path);

	sprintf(&pBuff[pathlength + 1], K2HFT_KEY_SUFFIX_FORM, pid);

	// length
	length = pathlength + 1 + strlen(&pBuff[pathlength + 1]) + 1;	// path + '\0' + pid + '\0'

	return reinterpret_cast<unsigned char*>(pBuff);
}

inline bool FreeK2hFtKey(unsigned char* pk2hftkey)
{
	if(!pk2hftkey){
		ERR_K2HFTPRN("pk2hftkey is NULL.");
		return false;
	}
	K2HFT_FREE(pk2hftkey);
	return true;
}

inline bool ParseK2hFtKey(unsigned char* pk2hftkey, char** path, pid_t* ppid)
{
	if(!pk2hftkey){
		ERR_K2HFTPRN("pk2hftkey is NULL.");
		return false;
	}
	if(path){
		*path = reinterpret_cast<char*>(pk2hftkey);
	}
	if(ppid){
		char*	pTmp= &(reinterpret_cast<char*>(pk2hftkey)[strlen(reinterpret_cast<char*>(pk2hftkey)) + 1]);
		*ppid		= static_cast<pid_t>(atoi(pTmp));
	}
	return true;
}

inline bool ParseK2hFtKey(unsigned char* pk2hftkey, char** path, char** ppid)
{
	if(!pk2hftkey){
		ERR_K2HFTPRN("pk2hftkey is NULL.");
		return false;
	}
	if(path){
		*path = reinterpret_cast<char*>(pk2hftkey);
	}
	if(ppid){
		*ppid = &(reinterpret_cast<char*>(pk2hftkey)[strlen(reinterpret_cast<char*>(pk2hftkey)) + 1]);
	}
	return true;
}

//
// Value format :	<K2HFTVHEAD><value data = <K2HFTLINE> * n>
//
inline PK2HFTVALUE BuildK2hFtValue(const k2hftlinelist_t& lines)
{
	if(lines.empty()){
		ERR_K2HFTPRN("data or length are empty.");
		return NULL;
	}

	// calac binary data length
	size_t	totallen = 0;
	for(k2hftlinelist_t::const_iterator iter = lines.begin(); iter != lines.end(); ++iter){
		const PK2HFTLINE	pline = *iter;
		totallen += (sizeof(K2HFTLINEHEAD) + pline->head.linelength);
	}

	// allocate
	unsigned char*	pBuff;
	if(NULL == (pBuff = reinterpret_cast<unsigned char*>(malloc(totallen + sizeof(K2HFTVHEAD))))){
		ERR_K2HFTPRN("Could not allocate memory.");
		return NULL;
	}

	// set head
	PK2HFTVALUE		pK2hFtVal	= reinterpret_cast<PK2HFTVALUE>(pBuff);
	pK2hFtVal->head.bodylength	= totallen;
	pK2hFtVal->head.linecount	= lines.size();

	// set body
	unsigned char*	pSetPos		= reinterpret_cast<unsigned char*>(&(pK2hFtVal->byValue[K2HFT_VALUE_BODY_START_POS]));
	for(k2hftlinelist_t::const_iterator iter = lines.begin(); iter != lines.end(); ++iter){
		// copy one line
		const PK2HFTLINE	pline = *iter;
		memcpy(pSetPos, &(pline->byLine[0]), (sizeof(K2HFTLINEHEAD) + pline->head.linelength));
		pSetPos = &pSetPos[sizeof(K2HFTLINEHEAD) + pline->head.linelength];
	}

	return pK2hFtVal;
}

inline unsigned char* GetK2hFtValuePtr(PK2HFTVALUE pk2hftval)
{
	if(!pk2hftval){
		ERR_K2HFTPRN("pk2hftval is NULL.");
		return NULL;
	}
	return &(pk2hftval->byValue[0]);
}

inline size_t GetK2hFtValueSize(PK2HFTVALUE pk2hftval)
{
	if(!pk2hftval){
		ERR_K2HFTPRN("pk2hftval is NULL.");
		return 0;
	}
	return (pk2hftval->head.bodylength + sizeof(K2HFTVHEAD));
}

inline bool FreeK2hFtValue(PK2HFTVALUE pk2hftval)
{
	if(!pk2hftval){
		ERR_K2HFTPRN("pk2hftval is NULL.");
		return false;
	}
	K2HFT_FREE(pk2hftval);
	return true;
}

inline PK2HFTLINE GetK2hFtLines(PK2HFTVALUE pk2hftval, size_t* pcount)
{
	if(!pk2hftval){
		ERR_K2HFTPRN("pk2hftval is NULL.");
		return NULL;
	}

	unsigned char*	pFirst		= GetK2hFtValuePtr(pk2hftval) + static_cast<off_t>(K2HFT_VALUE_BODY_START_POS);
	size_t			AllLength	= GetK2hFtValueSize(pk2hftval);
	if(!pFirst || AllLength < sizeof(K2HFTLINEHEAD)){
		ERR_K2HFTPRN("There is no lines data in value.");
		return NULL;
	}
	if(pcount){
		*pcount = pk2hftval->head.linecount;
	}
	return reinterpret_cast<PK2HFTLINE>(pFirst);
}

//
// Line format :	<K2HFTLINEHEAD><line data>
//
inline PK2HFTLINE BuildK2hFtLine(const unsigned char* data, size_t length, pid_t pid)
{
	if(!data || 0 == length){
		ERR_K2HFTPRN("data or length are empty.");
		return NULL;
	}
	unsigned char*	pBuff;
	if(NULL == (pBuff = reinterpret_cast<unsigned char*>(malloc(length + sizeof(K2HFTLINEHEAD))))){
		ERR_K2HFTPRN("Could not allocate memory.");
		return NULL;
	}
	PK2HFTLINE		pK2hFtLine	= reinterpret_cast<PK2HFTLINE>(pBuff);
	pK2hFtLine->head.linelength	= length;
	pK2hFtLine->head.pid		= pid;
	get_local_hostname(pK2hFtLine->head.hostname);

	// [NOTE]
	// ts is a member of packed structure, thus we need temporary buffer for unable using reference to ts.
	//
	struct timespec	tmpts;
	if(!set_now_timespec(tmpts)){
		ERR_K2HFTPRN("Could not get timespec for now, thus using common init time.");
		tmpts = common_init_time;
	}
	pK2hFtLine->head.ts			= tmpts;

	memcpy(&pBuff[K2HFT_LINE_BODY_START_POS], data, length);

	return pK2hFtLine;
}

inline unsigned char* GetK2hFtLinePtr(PK2HFTLINE pk2hftline)
{
	if(!pk2hftline){
		ERR_K2HFTPRN("pk2hftline is NULL.");
		return NULL;
	}
	return &(pk2hftline->byLine[0]);
}

inline size_t GetK2hFtLineSize(PK2HFTLINE pk2hftline)
{
	if(!pk2hftline){
		ERR_K2HFTPRN("pk2hftline is NULL.");
		return 0;
	}
	return (pk2hftline->head.linelength + sizeof(K2HFTLINEHEAD));
}

inline bool FreeK2hFtLine(PK2HFTLINE pk2hftline)
{
	if(!pk2hftline){
		ERR_K2HFTPRN("pk2hftline is NULL.");
		return false;
	}
	K2HFT_FREE(pk2hftline);
	return true;
}

inline PK2HFTLINE GetNextK2hFtLine(PK2HFTLINE pk2hftline)
{
	if(!pk2hftline){
		ERR_K2HFTPRN("pk2hftline is NULL.");
		return NULL;
	}
	size_t	curlength = GetK2hFtLineSize(pk2hftline);
	if(curlength <= 0){
		return NULL;
	}
	unsigned char*	pNext = GetK2hFtLinePtr(pk2hftline);

	return reinterpret_cast<PK2HFTLINE>(&pNext[curlength]);
}

inline bool ParseK2hFtLine(PK2HFTLINE pk2hftline, unsigned char** data, size_t* plength, char** hostname, struct timespec* pts, pid_t* ppid)
{
	if(!pk2hftline){
		ERR_K2HFTPRN("pk2hftline is NULL.");
		return false;
	}
	if(plength){
		*plength = pk2hftline->head.linelength;
	}
	if(data){
		if(0 < pk2hftline->head.linelength){
			*data = reinterpret_cast<unsigned char*>(&(pk2hftline->byLine[K2HFT_LINE_BODY_START_POS]));
		}else{
			*data = NULL;
		}
	}
	if(ppid){
		*ppid = pk2hftline->head.pid;
	}
	if(hostname){
		*hostname = &(pk2hftline->head.hostname[0]);
	}
	if(pts){
		*pts = pk2hftline->head.ts;
	}
	return true;
}

#endif	// K2HFTSTRUCTURE_H

/*
 * VIM modelines
 *
 * vim:set ts=4 fenc=utf-8:
 */
