#include "GDKX.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

YYRunnerInterface gs_runnerInterface;
YYRunnerInterface* g_pYYRunnerInterface;


__declspec (dllexport) bool YYExtensionInitialise(const struct YYRunnerInterface* _pFunctions, size_t _functions_size)
{
	if (_functions_size < sizeof(YYRunnerInterface)) {
		printf("ERROR : runner interface mismatch in extension DLL\n ");
	} // end if

	// copy out all the functions 
	memcpy(&gs_runnerInterface, _pFunctions, _functions_size);
	g_pYYRunnerInterface = &gs_runnerInterface;

	return true;
}

__declspec (dllexport) void Func(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	double param = argc >= 1 ? YYGetReal(arg, 0) : -1.0;
	printf("Hello World %f\n", param);


	YYError("I am groot");
}
