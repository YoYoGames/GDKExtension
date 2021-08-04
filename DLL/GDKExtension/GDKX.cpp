#include "GDKX.h"

#ifdef YYXBOX
/* Building as part of the runner */

#include <stdarg.h>
#include <stdio.h>

#include "Files/Function/Function_Data_Structures.h"
#include "Files/Support/Support_Data_Structures.h"

static void DebugConsoleWrapper(const char* fmt, ...)
{
	va_list argv;

	va_start(argv, fmt);
	int len = vsnprintf(NULL, 0, fmt, argv);
	assert(len >= 0);
	va_end(argv);

	char* buf = (char*)(YYAlloc(len + 1));

	va_start(argv, fmt);
	vsnprintf(buf, len + 1, fmt, argv);
	va_end(argv);

	dbg_csol.Output("%s", buf);

	YYFree(buf);
}

void (*GDKX::DebugConsoleOutput)(const char* fmt, ...) = &DebugConsoleWrapper;
void (*GDKX::ShowMessage)(const char* msg) = &::ShowMessage;

void (*GDKX::CreateAsyncEventWithDSMap)(int, int) = &CreateAsynEventWithDSMap;
int(*GDKX::CreateDsMap)(int _num, ...) = &::CreateDsMap;

static bool DsMapAddI64Wrapper(int _index, const char* _pKey, int64 value)
{
	return F_DsMapAdd_Internal(_index, _pKey, value, false);
}

bool (*GDKX::DsMapAddDouble)(int _index, const char* _pKey, double value) = &F_DsMapAdd_Internal;
bool (*GDKX::DsMapAddString)(int _index, const char* _pKey, const char* pVal) = &F_DsMapAdd_Internal;
bool (*GDKX::DsMapAddInt64)(int _index, const char* _pKey, int64_t value) = &DsMapAddI64Wrapper;

#else
#include <stdlib.h>
#include <stdio.h>

#include "Extension_Interface.h"

struct RValue;
class CInstance;

/* Building as extension */

void (*GDKX::CreateAsyncEventWithDSMap)(int, int) = NULL;
int(*GDKX::CreateDsMap)(int _num, ...) = NULL;

bool (*GDKX::DsMapAddDouble)(int _index, const char* _pKey, double value) = NULL;
bool (*GDKX::DsMapAddString)(int _index, const char* _pKey, const char* pVal) = NULL;
bool (*GDKX::DsMapAddInt64)(int _index, const char* _pKey, int64_t value) = NULL;

#if 0
__declspec (dllexport) void RegisterRuntimeFunctions(char* arg1, char* arg2, char* arg3, char* arg4, char* arg5)
{
	GDKX::CreateAsynEventWithDSMap = (void (*)(int, int))(arg1);
	GDKX::CreateDsMap = (int(*)(int _num, ...))(arg2);
	GDKX::DsMapAddDouble = (bool(*)(int, const char*, double))(arg3);
	GDKX::DsMapAddString = (bool(*)(int, const char*, const char*))(arg4);
	GDKX::DsMapAddInt64 = (bool(*)(int, const char*, int64_t))(arg5);
}
#endif

__declspec (dllexport) bool ExtensionInitialise(const struct GameMaker_RunnerInterface* functions, size_t functions_size)
{
	GameMaker_RequireFunction(functions, functions_size, CreateAsyncEventWithDSMap)
	return true;
}

__declspec (dllexport) void Func(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	printf("Hello World\n");
}
#endif
