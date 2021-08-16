//
// Copyright (C) 2020 Opera Norway AS. All rights reserved.
//
// This file is an original work developed by Opera.
//

#ifndef __YY__RUNNER_INTERFACE_H_
#define __YY__RUNNER_INTERFACE_H_

#include <stdint.h>

struct RValue;
class YYObjectBase;
class CInstance;
struct YYRunnerInterface;
typedef void (*TSetRunnerInterface)(const YYRunnerInterface* pRunnerInterface, size_t _functions_size);
typedef void (*TYYBuiltin)(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg);
typedef long long int64;
typedef unsigned long long uint64;
typedef int32_t int32;
typedef uint32_t uint32;
typedef int16_t int16;
typedef uint16_t uint16;
typedef int8_t int8;
typedef uint8_t uint8;

typedef void* HYYMUTEX;

struct YYRunnerInterface
{
	// basic interaction with the user
	void (*DebugConsoleOutput)(const char* fmt, ...); // hook to YYprintf
	void (*ShowMessage)(const char* msg);

	// for printing error messages
	void (*YYError)(const char* _error, ...);

	// alloc, realloc and free
	void* (*YYAlloc)(int _size);
	void* (*YYRealloc)(void* pOriginal, int _newSize);
	void  (*YYFree)(const void* p);
	const char* (*YYStrDup)(const char* _pS);

	// yyget* functions for parsing arguments out of the arg index
	bool (*YYGetBool)(const RValue* _pBase, int _index);
	float (*YYGetFloat)(const RValue* _pBase, int _index);
	double (*YYGetReal)(const RValue* _pBase, int _index);
	int32_t(*YYGetInt32)(const RValue* _pBase, int _index);
	uint32_t(*YYGetUint32)(const RValue* _pBase, int _index);
	int64(*YYGetInt64)(const RValue* _pBase, int _index);
	void* (*YYGetPtr)(const RValue* _pBase, int _index);
	intptr_t(*YYGetPtrOrInt)(const RValue* _pBase, int _index);
	const char* (*YYGetString)(const RValue* _pBase, int _index);

	// typed get functions from a single rvalue
	bool (*BOOL_RValue)(const RValue* _pValue);
	double (*REAL_RValue)(const RValue* _pValue);
	void* (*PTR_RValue)(const RValue* _pValue);
	int64(*INT64_RValue)(const RValue* _pValue);
	int32_t(*INT32_RValue)(const RValue* _pValue);

	// calculate hash values from an RValue
	int (*HASH_RValue)(const RValue* _pValue);

	// copying, setting and getting RValue
	void (*SET_RValue)(RValue* _pDest, RValue* _pV, YYObjectBase* _pPropSelf, int _index);
	bool (*GET_RValue)(RValue* _pRet, RValue* _pV, YYObjectBase* _pPropSelf, int _index, bool fPrepareArray, bool fPartOfSet);
	void (*COPY_RValue)(RValue* _pDest, const RValue* _pSource);
	int (*KIND_RValue)(const RValue* _pValue);
	void (*FREE_RValue)(RValue* _pValue);
	void (*YYCreateString)(RValue* _pVal, const char* _pS);

	void (*YYCreateArray)(RValue* pRValue, int n_values, const double* values);

	// finding and runnine user scripts from name
	int (*Script_Find_Id)(const char* name);
	bool (*Script_Perform)(int ind, CInstance* selfinst, CInstance* otherinst, int argc, RValue* res, RValue* arg);

	// finding builtin functions
	bool  (*Code_Function_Find)(const char* name, int* ind);

	// timing
	int64(*Timing_Time)(void);

	// mutex handling
	HYYMUTEX (*YYMutexCreate)(const char* _name);
	void (*YYMutexDestroy)(HYYMUTEX hMutex);
	void (*YYMutexLock)(HYYMUTEX hMutex);
	void (*YYMutexUnlock)(HYYMUTEX hMutex);

	// ds map manipulation for 
	void (*CreateAsyncEventWithDSMap)(int _map, int _event);
	int (*CreateDsMap)(int _num, ...);

	bool (*DsMapAddDouble)(int _index, const char* _pKey, double value);
	bool (*DsMapAddString)(int _index, const char* _pKey, const char* pVal);
	bool (*DsMapAddInt64)(int _index, const char* _pKey, int64 value);

	// buffer access
	const void* (*BufferGetContent)(int _index);
	int (*BufferGetContentSize)(int _index);

	// variables
	volatile bool* pLiveConnection;
	int* pHTTP_ID;
};


#if defined(__YYDEFINE_EXTENSION_FUNCTIONS__)
extern YYRunnerInterface* g_pYYRunnerInterface;

// basic interaction with the user
#define DebugConsoleOutput(fmt, ...)		g_pYYRunnerInterface->DebugConsoleOutput(fmt, __VA_ARGS__  )
inline void ShowMessage(const char* msg) { g_pYYRunnerInterface->ShowMessage(msg); }

// for printing error messages
#define YYError(_error, ...)				g_pYYRunnerInterface->YYError( _error, __VA_ARGS__ )

// alloc, realloc and free
inline void* YYAlloc(int _size) { return g_pYYRunnerInterface->YYAlloc(_size); }
inline void* YYRealloc(void* pOriginal, int _newSize) { return g_pYYRunnerInterface->YYRealloc(pOriginal, _newSize); }
inline void  YYFree(const void* p) { g_pYYRunnerInterface->YYFree(p); }
inline const char* YYStrDup(const char* _pS) { return g_pYYRunnerInterface->YYStrDup(_pS); }

// yyget* functions for parsing arguments out of the arg index
inline bool YYGetBool(const RValue* _pBase, int _index) { return g_pYYRunnerInterface->YYGetBool(_pBase, _index); }
inline float YYGetFloat(const RValue* _pBase, int _index) { return g_pYYRunnerInterface->YYGetFloat(_pBase, _index); }
inline double YYGetReal(const RValue* _pBase, int _index) { return g_pYYRunnerInterface->YYGetReal(_pBase, _index); }
inline int32_t YYGetInt32(const RValue* _pBase, int _index) { return g_pYYRunnerInterface->YYGetInt32(_pBase, _index); }
inline uint32_t YYGetUint32(const RValue* _pBase, int _index) { return g_pYYRunnerInterface->YYGetUint32(_pBase, _index); }
inline int64 YYGetInt64(const RValue* _pBase, int _index) { return g_pYYRunnerInterface->YYGetInt64(_pBase, _index); }
inline void* YYGetPtr(const RValue* _pBase, int _index) { return g_pYYRunnerInterface->YYGetPtr(_pBase, _index); }
inline intptr_t YYGetPtrOrInt(const RValue* _pBase, int _index) { return g_pYYRunnerInterface->YYGetPtrOrInt(_pBase, _index); }
inline const char* YYGetString(const RValue* _pBase, int _index) { return g_pYYRunnerInterface->YYGetString(_pBase, _index); }

// typed get functions from a single rvalue
inline bool BOOL_RValue(const RValue* _pValue) { return g_pYYRunnerInterface->BOOL_RValue(_pValue); }
inline double REAL_RValue(const RValue* _pValue) { return g_pYYRunnerInterface->REAL_RValue(_pValue); }
inline void* PTR_RValue(const RValue* _pValue) { return g_pYYRunnerInterface->PTR_RValue(_pValue); }
inline int64 INT64_RValue(const RValue* _pValue) { return g_pYYRunnerInterface->INT64_RValue(_pValue); }
inline int32_t INT32_RValue(const RValue* _pValue) { return g_pYYRunnerInterface->INT32_RValue(_pValue); }

// calculate hash values from an RValue
inline int HASH_RValue(const RValue* _pValue) { return g_pYYRunnerInterface->HASH_RValue(_pValue); }

// copying, setting and getting RValue
inline void SET_RValue(RValue* _pDest, RValue* _pV, YYObjectBase* _pPropSelf, int _index) { return g_pYYRunnerInterface->SET_RValue(_pDest, _pV, _pPropSelf, _index); }
inline bool GET_RValue(RValue* _pRet, RValue* _pV, YYObjectBase* _pPropSelf, int _index, bool fPrepareArray = false, bool fPartOfSet = false) { return g_pYYRunnerInterface->GET_RValue(_pRet, _pV, _pPropSelf, _index, fPrepareArray, fPartOfSet); }
inline void COPY_RValue(RValue* _pDest, const RValue* _pSource) { g_pYYRunnerInterface->COPY_RValue(_pDest, _pSource); }
inline int KIND_RValue(const RValue* _pValue) { return g_pYYRunnerInterface->KIND_RValue(_pValue); }
inline void FREE_RValue(RValue* _pValue) { return g_pYYRunnerInterface->FREE_RValue(_pValue); }
inline void YYCreateString(RValue* _pVal, const char* _pS) { g_pYYRunnerInterface->YYCreateString(_pVal, _pS); }

inline void YYCreateArray(RValue* pRValue, int n_values = 0, const double* values = NULL) { g_pYYRunnerInterface->YYCreateArray(pRValue, n_values, values); }

// finding and runnine user scripts from name
inline int Script_Find_Id(char* name) { return g_pYYRunnerInterface->Script_Find_Id(name); }
inline bool Script_Perform(int ind, CInstance* selfinst, CInstance* otherinst, int argc, RValue* res, RValue* arg) {
	return g_pYYRunnerInterface->Script_Perform(ind, selfinst, otherinst, argc, res, arg);
}

// finding builtin functions
inline bool  Code_Function_Find(char* name, int* ind) { return g_pYYRunnerInterface->Code_Function_Find(name, ind); }

// timing
inline int64 Timing_Time(void) { return g_pYYRunnerInterface->Timing_Time(); }

// mutex functions
inline HYYMUTEX YYMutexCreate(const char* _name) { return g_pYYRunnerInterface->YYMutexCreate(_name); }
inline void YYMutexDestroy(HYYMUTEX hMutex) { g_pYYRunnerInterface->YYMutexDestroy(hMutex); }
inline void YYMutexLock(HYYMUTEX hMutex) { g_pYYRunnerInterface->YYMutexLock(hMutex); }
inline void YYMutexUnlock(HYYMUTEX hMutex) { g_pYYRunnerInterface->YYMutexUnlock(hMutex); }

// ds map manipulation for 
inline void CreateAsyncEventWithDSMap(int _map, int _event) { return g_pYYRunnerInterface->CreateAsyncEventWithDSMap(_map, _event); }
#define CreateDsMap(_num, ...) g_pYYRunnerInterface->CreateDsMap( _num, __VA_ARGS__ )

inline bool DsMapAddDouble(int _index, const char* _pKey, double value) { return g_pYYRunnerInterface->DsMapAddDouble(_index, _pKey, value); }
inline bool DsMapAddString(int _index, const char* _pKey, const char* pVal) { return g_pYYRunnerInterface->DsMapAddString(_index, _pKey, pVal); }
inline bool DsMapAddInt64(int _index, const char* _pKey, int64 value) { return g_pYYRunnerInterface->DsMapAddInt64(_index, _pKey, value); }

// buffer access
inline const void* BufferGetContent(int _index) { return g_pYYRunnerInterface->BufferGetContent(_index); }
inline int BufferGetContentSize(int _index) { return g_pYYRunnerInterface->BufferGetContentSize(_index); }

#define g_LiveConnection	(*g_pYYRunnerInterface->pLiveConnection)
#define g_HTTP_ID			(*g_pYYRunnerInterface->pHTTP_ID)


#endif


/*
#define YY_HAS_FUNCTION(interface, interface_size, function) \
	(interface_size >= (offsetof(GameMaker_RunnerInterface, function) + sizeof(GameMaker_RunnerInterface::function)) && interface->function != NULL)

#define YY_REQUIRE_FUNCTION(interface, interface_size, function) \
	if(!GameMaker_HasFunction(interface, interface_size, function)) \
	{ \
		interface->DebugConsoleOutput("Required function missing: %s\n", #function); \
		interface->DebugConsoleOutput("This extension may not be compatible with this version of GameMaker\n"); \
		return false; \
	}
*/

#endif
