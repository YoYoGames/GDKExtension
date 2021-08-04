#include "GDKX.h"
#include <stdlib.h>
#include <stdio.h>

#include "Extension_Interface.h"

struct RValue;
class CInstance;


__declspec (dllexport) bool YYExtensionInitialise(const struct YYRunnerInterface* _pFunctions, size_t _functions_size)
{
	if (_functions_size < sizeof(YYRunnerInterface)) {
		printf("ERROR : runner interface mismatch in extension DLL\n ");
	} // end if

	// copy out all the functions 


	return true;
}

__declspec (dllexport) void Func(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	printf("Hello World\n");
}
