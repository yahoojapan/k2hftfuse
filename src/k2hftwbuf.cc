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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <sys/types.h>

#include "k2hftcommon.h"
#include "k2hftwbuf.h"
#include "k2hftman.h"
#include "k2hftutil.h"
#include "k2hftdbg.h"

using namespace std;

//---------------------------------------------------------
// Class K2hFtBinBuff : Variables
//---------------------------------------------------------
const size_t	K2hFtBinBuff::BUFFER_SINGLE_SIZE;

//---------------------------------------------------------
// Class K2hFtBinBuff : Methods
//---------------------------------------------------------
K2hFtBinBuff::K2hFtBinBuff(void) : CacheBuff(NULL), CacheBuffSize(0), CacheBuffPos(0)
{
}

K2hFtBinBuff::~K2hFtBinBuff(void)
{
	K2HFT_FREE(CacheBuff);
}

//
// This method allows null data(0 length).
//
bool K2hFtBinBuff::AppendStringBuff(const unsigned char* data, size_t length, bool check_last_char)
{
	// check buffer size
	if(CacheBuffSize < (CacheBuffPos + length + 1)){			// for '\0' = +1
		// buffer over flow, so reallocate
		CacheBuffSize += max(K2hFtBinBuff::BUFFER_SINGLE_SIZE, (length + 1));

		unsigned char*	TmpBuff;
		if(NULL == (TmpBuff = reinterpret_cast<unsigned char*>(realloc(CacheBuff, CacheBuffSize)))){
			ERR_K2HFTPRN("Could not allocate memory.");
			return false;
		}
		CacheBuff = TmpBuff;
	}
	// append data
	if(data && 0 < length){
		memcpy(&CacheBuff[CacheBuffPos], data, length);
		CacheBuffPos += length;
	}
	// check last character 
	if(check_last_char && '\0' != CacheBuff[CacheBuffPos - 1]){
		CacheBuff[CacheBuffPos++] = '\0';						// add '\0'
	}
	return true;
}

bool K2hFtBinBuff::TakeoutBuff(unsigned char** data, size_t* length)
{
	if(!data || !length){
		ERR_K2HFTPRN("Parameters are wrong.");
		return false;
	}
	*data	= CacheBuff;
	*length	= CacheBuffPos;

	CacheBuff		= NULL;
	CacheBuffSize	= 0;
	CacheBuffPos	= 0;

	return true;
}

//---------------------------------------------------------
// Class K2hFtWriteBuff : Variable
//---------------------------------------------------------
const size_t	K2hFtWriteBuff::DEFAULT_LINE_LIMIT;
const size_t	K2hFtWriteBuff::BUFFER_BINARY_SIZE;
const size_t	K2hFtWriteBuff::DEFAULT_LINE_MAX;
const time_t	K2hFtWriteBuff::DEFAULT_TIMEUP;
size_t			K2hFtWriteBuff::BinaryBuffLimit	= K2hFtWriteBuff::BUFFER_BINARY_SIZE;
size_t			K2hFtWriteBuff::LineByteLimit	= K2hFtWriteBuff::DEFAULT_LINE_LIMIT;
size_t			K2hFtWriteBuff::StackLineMax	= K2hFtWriteBuff::DEFAULT_LINE_MAX;
time_t			K2hFtWriteBuff::StackTimeup		= K2hFtWriteBuff::DEFAULT_TIMEUP;

//---------------------------------------------------------
// Class K2hFtWriteBuff
//---------------------------------------------------------
K2hFtWriteBuff::K2hFtWriteBuff(const K2hFtManage* pman, uint64_t fhandle) : pK2hFtMan(pman), filehandle(fhandle), inputbuf_lockval(FLCK_NOSHARED_MUTEX_VAL_UNLOCKED), stack_lockval(FLCK_NOSHARED_MUTEX_VAL_UNLOCKED)
{
	assert(pK2hFtMan && 0 != filehandle);
}

K2hFtWriteBuff::~K2hFtWriteBuff(void)
{
	Clean();
}

void K2hFtWriteBuff::Clean(void)
{
	while(!fullock::flck_trylock_noshared_mutex(&inputbuf_lockval));		// LOCK

	free_k2hftbinbufmap(InputBuffMap);
	free_k2hftlinelist(Stack);

	fullock::flck_unlock_noshared_mutex(&inputbuf_lockval);					// UNLOCK
}

K2hFtBinBuff* K2hFtWriteBuff::GetBinBuff(pid_t pid)
{
	while(!fullock::flck_trylock_noshared_mutex(&inputbuf_lockval));		// LOCK

	K2hFtBinBuff*				pResult;
	k2hftbinbufmap_t::iterator	iter = InputBuffMap.find(pid);
	if(InputBuffMap.end() == iter || NULL == (pResult = iter->second)){
		pResult = new K2hFtBinBuff();
		InputBuffMap[pid] = pResult;
	}
	fullock::flck_unlock_noshared_mutex(&inputbuf_lockval);					// UNLOCK

	return pResult;
}

// [NOTE]
// If data and length are empty, the input buffer forces to put stack.
//
bool K2hFtWriteBuff::StringPush(K2hFtBinBuff* pBuff, const unsigned char* data, size_t length, pid_t pid)
{
	assert(pBuff);

	bool	force_eol = false;
	if(!data || 0 == length){
		force_eol	= true;
		length		= 0;
		data		= NULL;
	}

	bool	found = false;
	size_t	start = 0;
	size_t	pos;
	size_t	addcr = 0;		// for cut CRLF
	for(start = 0, pos = 0; pos < length; ++pos){
		// check end of line
		if('\n' == data[pos]){									// \n
			found = true;
		}else if('\r' == data[pos]){
			if((pos + 1) < length && '\n' == data[pos + 1]){	// \r\n
				addcr = 1;
				found = true;
			}else{												// \r
				found = true;
			}
		}else if('\0' == data[pos]){							// \0
			found = true;
		}

		if(found){
			// append
			if(!pBuff->AppendStringBuff(&data[start], (pos - start + 1), true)){	// with checking last character
				ERR_K2HFTPRN("Failed to append binary data to internal buffer.");
				return false;
			}
			// take out buffer
			unsigned char*	bufdata;
			size_t			buflength;
			if(!pBuff->TakeoutBuff(&bufdata, &buflength)){
				ERR_K2HFTPRN("Failed to take out buffer.");
				return false;
			}
			// add stack
			if(!StackPush(bufdata, buflength, pid)){
				ERR_K2HFTPRN("Failed to stack one line data, but continue...");
			}
			// set next start pos
			pos  += addcr;
			start = pos + 1;
			addcr = 0;
			found = false;
		}
	}

	// lest
	if(start < length){
		if(!pBuff->AppendStringBuff(&data[start], (length - start), false)){				// without checking last character
			ERR_K2HFTPRN("Failed to append binary data to internal buffer.");
			return false;
		}
	}

	// force
	if(force_eol && !pBuff->IsEmpty()){
		// append last character
		if(!pBuff->AppendStringBuff(NULL, 0, true)){								// with checking last character
			ERR_K2HFTPRN("Failed to append binary data to internal buffer.");
			return false;
		}
		// take out buffer
		unsigned char*	bufdata;
		size_t			buflength;
		if(!pBuff->TakeoutBuff(&bufdata, &buflength)){
			ERR_K2HFTPRN("Failed to take out buffer.");
			return false;
		}
		// add stack
		if(!StackPush(bufdata, buflength, pid)){
			ERR_K2HFTPRN("Failed to stack one line data, but continue...");
		}
	}
	return true;
}

// [NOTE]
// If data and length are empty, the input buffer forces to put stack.
//
bool K2hFtWriteBuff::BinaryPush(K2hFtBinBuff* pBuff, const unsigned char* data, size_t length, pid_t pid)
{
	assert(pBuff);

	bool	force_eol = false;
	if(!data || 0 == length){
		force_eol = true;
	}

	for(size_t pos = 0; pos < length; ){
		size_t	nowsize = pBuff->Size();
		size_t	maxsize = K2hFtWriteBuff::BinaryBuffLimit - nowsize;
		size_t	setsize = min(maxsize, (length - pos));

		// append
		if(!pBuff->AppendStringBuff(&data[pos], setsize, false)){		// without checking last character
			ERR_K2HFTPRN("Failed to append binary data to internal buffer.");
			return false;
		}

		// check push
		if(K2hFtWriteBuff::BinaryBuffLimit <= pBuff->Size()){
			// take out buffer
			unsigned char*	bufdata;
			size_t			buflength;
			if(!pBuff->TakeoutBuff(&bufdata, &buflength)){
				ERR_K2HFTPRN("Failed to take out buffer.");
				return false;
			}
			// add stack
			if(!StackPush(bufdata, buflength, pid)){
				ERR_K2HFTPRN("Failed to stack one line data, but continue...");
			}
		}
		pos += setsize;
	}
	// force
	if(force_eol && !pBuff->IsEmpty()){
		// take out buffer
		unsigned char*	bufdata;
		size_t			buflength;
		if(!pBuff->TakeoutBuff(&bufdata, &buflength)){
			ERR_K2HFTPRN("Failed to take out buffer.");
			return false;
		}
		// add stack
		if(!StackPush(bufdata, buflength, pid)){
			ERR_K2HFTPRN("Failed to stack one line data, but continue...");
		}
	}
	return true;
}

bool K2hFtWriteBuff::StackPush(unsigned char* data, size_t length, pid_t pid)
{
	if(!data || 0 == length){
		// nothing to stack
		K2HFT_FREE(data);
		return true;
	}

	// convert output data
	size_t			cvtlength	= 0;
	unsigned char*	pOutput		= pK2hFtMan->CvtPushData(data, length, filehandle, cvtlength);
	if(!pOutput){
		// nothing to put data
		K2HFT_FREE(data);
		return true;
	}

	// check limit
	if(K2hFtWriteBuff::DEFAULT_LINE_LIMIT != K2hFtWriteBuff::LineByteLimit && K2hFtWriteBuff::LineByteLimit < cvtlength){
		MSG_K2HFTPRN("data length(%zu) after converting is over line byte limit(%zu), so this data is not processed.", cvtlength, K2hFtWriteBuff::LineByteLimit);

		if(pOutput != data){		// [NOTICE]
			K2HFT_FREE(pOutput);
		}
		K2HFT_FREE(data);
		return true;
	}

	// build one line data
	PK2HFTLINE	pLine = BuildK2hFtLine(pOutput, cvtlength, pid);
	if(pLine){
		// stack
		while(!fullock::flck_trylock_noshared_mutex(&stack_lockval));			// LOCK
		Stack.push_back(pLine);
		fullock::flck_unlock_noshared_mutex(&stack_lockval);					// UNLOCK
	}

	// free
	if(pOutput != data){		// [NOTICE]
		K2HFT_FREE(pOutput);
	}
	// cppcheck-suppress uselessAssignmentPtrArg
	K2HFT_FREE(data);

	return true;
}

bool K2hFtWriteBuff::Push(const unsigned char* data, size_t length, bool is_binary_mode, pid_t pid)
{
	bool	result = true;

	if(K2HFT_INVALID_PID == pid){
		// flush all buffers
		while(!fullock::flck_trylock_noshared_mutex(&inputbuf_lockval));		// LOCK

		for(k2hftbinbufmap_t::iterator iter = InputBuffMap.begin(); iter != InputBuffMap.end(); InputBuffMap.erase(iter++)){
			pid_t			pid_map	= iter->first;
			K2hFtBinBuff*	pBuff	= iter->second;
			if(pBuff){
				if(is_binary_mode){
					if(!BinaryPush(pBuff, data, length, pid_map)){
						result = false;
					}
				}else{
					if(!StringPush(pBuff, data, length, pid_map)){
						result = false;
					}
				}
				K2HFT_DELETE(pBuff);
			}
		}
		fullock::flck_unlock_noshared_mutex(&inputbuf_lockval);					// UNLOCK

	}else{
		K2hFtBinBuff*	pBuff = GetBinBuff(pid);
		if(is_binary_mode){
			result = BinaryPush(pBuff, data, length, pid);
		}else{
			result = StringPush(pBuff, data, length, pid);
		}
	}
	return result;
}

bool K2hFtWriteBuff::IsStackLimit(void)
{
	// no lock for fast
	size_t	count = Stack.size();
	if(K2hFtWriteBuff::StackLineMax < count){
		return true;
	}

	while(!fullock::flck_trylock_noshared_mutex(&stack_lockval));			// LOCK

	bool	result = true;
	count = Stack.size();
	if(0 == count){
		result = false;
	}else if(count <= K2hFtWriteBuff::StackLineMax){
		PK2HFTLINE	pline = Stack.front();
		if(pline && time(NULL) <= (pline->head.ts.tv_sec + K2hFtWriteBuff::StackTimeup)){
			result = false;
		}
	}
	fullock::flck_unlock_noshared_mutex(&stack_lockval);					// UNLOCK

	return result;
}

//
// Need to free() for return value.
//
PK2HFTVALUE K2hFtWriteBuff::Pop(void)
{
	k2hftlinelist_t	TmpStack;

	// copy stack
	while(!fullock::flck_trylock_noshared_mutex(&stack_lockval));			// LOCK
	while(!Stack.empty()){
		PK2HFTLINE	pline = Stack.front();
		Stack.erase(Stack.begin());
		TmpStack.push_back(pline);
	}
	fullock::flck_unlock_noshared_mutex(&stack_lockval);					// UNLOCK

	if(TmpStack.empty()){
		//MSG_K2HFTPRN("Stack is empty.");
		return NULL;
	}

	// make k2hash value
	PK2HFTVALUE	pValue = BuildK2hFtValue(TmpStack);

	// free stack(temp)
	free_k2hftlinelist(TmpStack);

	return pValue;
}

/*
 * VIM modelines
 *
 * vim:set ts=4 fenc=utf-8:
 */
