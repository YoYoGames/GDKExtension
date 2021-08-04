#pragma once

#include <assert.h>
#include <stdint.h>

#include "Extension_Interface.h"

#ifdef YYXBOX
#include "../../Files/Object/Object_Class.h"
#else
#define YYASSERT(...) assert(__VA_ARGS__)

static const int EVENT_OTHER_DIALOG_ASYNC = 63;
static const int EVENT_OTHER_SYSTEM_EVENT = 75;
#endif

namespace GDKX
{
	extern void (*DebugConsoleOutput)(const char* fmt, ...);
	extern void (*ShowMessage)(const char* msg);

	extern void (*CreateAsyncEventWithDSMap)(int, int);
	extern int(*CreateDsMap)(int _num, ...);

	extern bool (*DsMapAddDouble)(int _index, const char* _pKey, double value);
	extern bool (*DsMapAddString)(int _index, const char* _pKey, const char* pVal);
	extern bool (*DsMapAddInt64)(int _index, const char* _pKey, int64_t value);
}
