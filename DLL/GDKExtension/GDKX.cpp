//
// Copyright (C) 2020 Opera Norway AS. All rights reserved.
//
// This file is an original work developed by Opera.
//

#include "GDKX.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <windows.h>
#include <XGameRuntime.h>

#include "Stats.h"
#include "UserManagement.h"

#define _GAMING_XBOX

YYRunnerInterface gs_runnerInterface;
YYRunnerInterface* g_pYYRunnerInterface;

char* g_XboxSCID = NULL;


YYEXPORT
bool YYExtensionInitialise(const struct YYRunnerInterface* _pFunctions, size_t _functions_size)
{
	if (_functions_size < sizeof(YYRunnerInterface)) {
		printf("ERROR : runner interface mismatch in extension DLL\n ");
	} // end if

	// copy out all the functions 
	memcpy(&gs_runnerInterface, _pFunctions, _functions_size);
	g_pYYRunnerInterface = &gs_runnerInterface;


	return true;
}

YYEXPORT
void gdk_init(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	if (argc != 1)
	{
		YYError("gdk_init() called with %d arguments, expected 1", argc);
		return;
	}

	YYFree(g_XboxSCID);
	g_XboxSCID = (char*)(YYStrDup(YYGetString(arg, 1)));

	XGameRuntimeInitialize();
	
	XblInitArgs xblArgs = {};
	xblArgs.scid = g_XboxSCID;

	HRESULT hr = XblInitialize(&xblArgs);
	if (FAILED(hr))
	{
		DebugConsoleOutput("Unable to initialize XSAPI (HRESULT 0x%08X)\n", (unsigned)(hr));
	}

	XUM::Init();
}

YYEXPORT
void gdk_update(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	XUM::Update();

	// Update stats
	XboxStatsManager::background_flush();
}

YYEXPORT
void gdk_quit(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	XUM::Quit();

	XAsyncBlock async;
	memset(&async, 0, sizeof(async));

	HRESULT status = XblCleanupAsync(&async);
	if (SUCCEEDED(status))
	{
		status = XAsyncGetStatus(&async, true);
	}

	if(!SUCCEEDED(status))
	{
		DebugConsoleOutput("XblCleanupAsync failed (HRESULT 0x%08X)\n", (unsigned)(status));
	}

	XGameRuntimeUninitialize();

	YYFree(g_XboxSCID);
	g_XboxSCID = NULL;
}

YYEXPORT
void F_XboxOneGetUserCount(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_REAL;
	Result.val = 0;

	int numusers = 0;
	XUM::GetUsers(numusers);

	Result.val = numusers;
}

YYEXPORT
void F_XboxOneGetUser(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_INT64;
	Result.v64 = 0;

	if ((argc != 1) || (arg[0].kind != VALUE_REAL))
	{
		YYError("xboxone_get_user() - argument should be a user number but it is not.", false);
		return;
	}

	int numusers = 0;
	XUMuser** users = XUM::GetUsers(numusers);

	int index = YYGetInt32(arg, 0);

	if ((index < 0) || (index >= numusers))
	{
		DebugConsoleOutput("xboxone_get_user() - index %d out of range\n", index);
		return;
	}

	// Hmmm - hate packing a 64bit value into a variable that is only 64bits on certain platforms (it works as these functions are all only on Xbox One) :/
	// Unfortunately can't use the proper 64bit value type as the user needs to compare against a value to check for success, and this literal will be interpreted as a double (and currently the types on each side of a comparison need to match)
	uint64 id = users[index]->XboxUserIdInt;
	Result.v64 = id;
}

YYEXPORT
void F_XboxOneGetActivatingUser(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_INT64;
	Result.v64 = 0;

	if (argc != 0)
	{
		YYError("xboxone_get_activating_user() - doesn't take any arguments", false);
		return;
	}
	XUM_LOCK_MUTEX;
	XUserLocalId user = XUM::GetActivatingUser();
	XUMuser* xuser = XUM::GetUserFromLocalId(user);
	if (xuser == NULL)
		return;

	Result.v64 = xuser->XboxUserIdInt;
}

YYEXPORT
void F_XboxOneAgeGroupForUser(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_REAL;
	Result.val = -1;


	uint64 user_id = (uint64)YYGetInt64(arg, 0);

	XUM_LOCK_MUTEX
	XUMuser* user = XUM::GetUserFromId(user_id);

	if (user != NULL)
	{
		Result.val = (double)(user->AgeGroup);
	}
	else {
		Result.val = 0.0;		// agegroup_unknown
		DebugConsoleOutput("xboxone_agegroup_for_user() - user not found", false);
	}

}

YYEXPORT
void F_XboxOnePadForUser(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_REAL;
	Result.val = -1;

	int padindex = YYGetInt32(arg, 1);

	XUM_LOCK_MUTEX;

	int numusers = 0;
	XUMuser** users = XUM::GetUsers(numusers);

	uint64 id = (uint64)YYGetInt64(arg, 0);

	for (int i = 0; i < numusers; i++)
	{
		XUMuser* user = users[i];
		if (user != NULL)
		{
			if (user->XboxUserIdInt == id)
			{
				// Get first valid gamepad
				//if (user->Controllers != NULL)
				{
					size_t numcontrollers = user->Controllers.size();
					for (int j = padindex; j < numcontrollers; j++)
					{
						APP_LOCAL_DEVICE_ID* deviceID = &(user->Controllers[j]);

						int padid = -1; // GetGamepadIndex(deviceID);
						if (padid != -1)		// this should never be -1
						{
							Result.val = padid;
							return;
						}
					}
				}
			}
		}
	}
	// No valid user found
	DebugConsoleOutput("xboxone_pad_for_user() - user not found", false);
}

YYEXPORT
void F_XboxOneShowAccountPicker(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_REAL;
	Result.val = -1;

	XUserAddOptions options = XUserAddOptions::None;
	if (argc == 0)
	{
		// UWP-style account picker
	}
	else
	{
		if ((argc < 2) || (arg[0].kind != VALUE_REAL) || (arg[1].kind != VALUE_REAL))
		{
			YYError("xboxone_show_account_picker() - invalid arguments", false);
			Result.val = -1;
			return;
		}

		int optionsval = YYGetInt32(arg, 1);
		if (optionsval == 1)
			options = XUserAddOptions::AllowGuests;
	}

	int ret = XUM::AddUser(options, true);
	if (ret < 0)
	{
		DebugConsoleOutput("xboxone_show_account_picker() failed\n");
		Result.val = ret;
	}
	else
	{
		Result.val = 0;
	}
}
