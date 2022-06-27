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
bool g_gdk_initialised = false;


XTaskQueueHandle g_taskQueue;
XTaskQueueRegistrationToken m_networkingConnectivityHintChangedCallbackToken;
void CALLBACK NetworkConnectivityHintChangedCallback(_In_opt_ void* context, _In_ const XNetworkingConnectivityHint* connectivityHint)
{
	bool isConnected = false;
	if ((connectivityHint->networkInitialized) && ((connectivityHint->connectivityLevel == XNetworkingConnectivityLevelHint::InternetAccess) || (connectivityHint->connectivityLevel == XNetworkingConnectivityLevelHint::ConstrainedInternetAccess)))
	{
		isConnected = true;
	}

	if (isConnected != g_LiveConnection)
	{
		g_LiveConnection = isConnected; // Update global state (avoid false re-triggers, the runner does it too)
		if (isConnected)
		{
			XUM::UpdateUserProfiles(true);	// try loading any failed profiles
		}
	}
}


void InitIAPFunctionsM();
void UpdateIAPFunctionsM();
void QuitIAPFunctionsM();

YYEXPORT
void YYExtensionInitialise(const struct YYRunnerInterface* _pFunctions, size_t _functions_size)
{
	if (_functions_size < sizeof(YYRunnerInterface)) {
		printf("ERROR : runner interface mismatch in extension DLL\n ");
	} // end if

	// copy out all the functions 
	memcpy(&gs_runnerInterface, _pFunctions, sizeof(YYRunnerInterface));
	g_pYYRunnerInterface = &gs_runnerInterface;
}

/* Find any files whose name matches filename_expr (may include wildcards) in
 * the same directory as the GDKExtension DLL.
 *
 * Returns a list of pathnames to any matching files.
*/
static std::vector<std::string> _find_packaged_files(const char *filename_expr)
{
	HMODULE this_dll;

	if (!GetModuleHandleEx(
		(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT),
		(LPCWSTR)(&_find_packaged_files),
		&this_dll))
	{
		DWORD err = GetLastError();
		DebugConsoleOutput("_find_packaged_files: Cannot get handle to GDKExtension DLL! (error code %u)\n", (unsigned)(err));

		return std::vector<std::string>();
	}

	char this_dll_path[MAX_PATH + 1];
	DWORD tdp_len = GetModuleFileNameA(this_dll, this_dll_path, (MAX_PATH + 1));

	if (tdp_len == 0 || tdp_len == (MAX_PATH + 1))
	{
		DWORD err = GetLastError();
		DebugConsoleOutput("_find_packaged_files: Cannot get path of GDK extension DLL! (error code %u)\n", (unsigned)(err));

		return std::vector<std::string>();
	}

	std::string this_dll_dir;

	{
		/* Set this_dll_dir to the directory portion of this_dll_path, including trailing slash
		 * ready to be prefixed to filename_expr and any found files.
		*/

		char* tdp_last_slash = strrchr(this_dll_path, '\\');

		if (tdp_last_slash != NULL)
		{
			size_t tdp_dir_len = (tdp_last_slash - this_dll_path) + 1;
			this_dll_dir = std::string(this_dll_path, tdp_dir_len);
		}
		else {
			/* Assume DLL is in current dir and no directory prefix is required... */
			this_dll_dir = "";
		}
	}

	std::vector<std::string> filenames;

	std::string path_expr = this_dll_dir + filename_expr;
	WIN32_FIND_DATAA ffd;
	HANDLE ffh = FindFirstFileA(path_expr.c_str(), &ffd);

	if (ffh != INVALID_HANDLE_VALUE)
	{
		do {
			filenames.push_back(this_dll_dir + ffd.cFileName);
		} while (FindNextFileA(ffh, &ffd));

		FindClose(ffh);
	}
	else {
		DWORD err = GetLastError();
		if (err != ERROR_FILE_NOT_FOUND)
		{
			DebugConsoleOutput("_find_packaged_files: Cannot find files (error code %u)\n", (unsigned)(err));
		}
	}

	return filenames;
}

YYEXPORT
void gdk_init(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	if (argc != 1)
	{
		YYError("gdk_init() called with %d arguments, expected 1", argc);
		return;
	}

	if (g_gdk_initialised)
	{
		DebugConsoleOutput("ERROR: gdk_init() called but GDK is already initialised!\n");
		return;
	}

	DebugConsoleOutput("gdk_init() called - initialising the GDK extension\n");

	std::vector<std::string> event_manifest_names = _find_packaged_files("Events-*.man");

	if (event_manifest_names.size() == 0)
	{
		DebugConsoleOutput("No event manifest found, event-based stats will not be available\n");
	}
	else if (event_manifest_names.size() > 1)
	{
		DebugConsoleOutput("Multiple event manifests found! Not loading any!\n");
	}
	else{
		DebugConsoleOutput("Loading event manifest %s...\n", event_manifest_names[0].c_str());
		Xbox_Stat_Load_XML(event_manifest_names[0].c_str());
	}	

	YYFree(g_XboxSCID);
	g_XboxSCID = (char*)(YYStrDup(YYGetString(arg, 0)));

	XGameRuntimeInitialize();

	// Create task queue (has to be done after XGameRuntimeInitialize() )
	HRESULT hr = XTaskQueueCreate(XTaskQueueDispatchMode::ThreadPool, XTaskQueueDispatchMode::Manual, &g_taskQueue);
	if (FAILED(hr))
	{
		YYError("XTaskQueueCreate failed (HRESULT 0x%08X)\n", (unsigned)(hr));
	}
	
	XblInitArgs xblArgs = {};
	xblArgs.scid = g_XboxSCID;

	hr = XblInitialize(&xblArgs);
	if (FAILED(hr))
	{
		DebugConsoleOutput("Unable to initialize XSAPI (HRESULT 0x%08X)\n", (unsigned)(hr));
	}

	XUM::Init();
	InitIAPFunctionsM();

	// CALLBACKS (Register)
	// Register a callback that will listen for changes in network connectivity
	XNetworkingRegisterConnectivityHintChanged(
		g_taskQueue,
		NULL,
		&NetworkConnectivityHintChangedCallback,
		&m_networkingConnectivityHintChangedCallbackToken
	);

	g_gdk_initialised = true;
}

YYEXPORT
void gdk_update(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	if (!g_gdk_initialised)
	{
		DebugConsoleOutput("ERROR: gdk_update() called but GDK isn't initialised! (call gdk_init() first)\n");
		return;
	}

	XUM::Update();
	UpdateIAPFunctionsM();

	// Handle network connection changed callback
	while (g_taskQueue != NULL && XTaskQueueDispatch(g_taskQueue, XTaskQueuePort::Completion, 0)) { }

	// Update stats
	XboxStatsManager::background_flush();
}

YYEXPORT
void gdk_quit(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	if (!g_gdk_initialised)
	{
		DebugConsoleOutput("ERROR: gdk_quit() called but GDK isn't initialised!\n");
		return;
	}

	DebugConsoleOutput("gdk_quit() called - shutting down the GDK extension\n");

	QuitIAPFunctionsM();
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

	// CALLBACKS (unregister)
	XNetworkingUnregisterConnectivityHintChanged(m_networkingConnectivityHintChangedCallbackToken, false);

	// Destroy task queue
	bool terminated = false;
	XTaskQueueTerminate(g_taskQueue, false, &terminated, [](void* context) { *(bool*)(context) = true; });
	while (XTaskQueueDispatch(g_taskQueue, XTaskQueuePort::Completion, 0) || !terminated) {}
	XTaskQueueCloseHandle(g_taskQueue);
	g_taskQueue = NULL;

	XGameRuntimeUninitialize();

	YYFree(g_XboxSCID);
	g_XboxSCID = NULL;

	g_gdk_initialised = false;
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
		int optionsval = YYGetInt32(arg, 1);
		if (optionsval == 1)
			options = XUserAddOptions::AllowGuests;
	}

	int ret = XUM::AddUser(options, true);
	if (ret < 0)
	{
		DebugConsoleOutput("xboxone_show_account_picker() failed\n");
	}

	Result.val = ret;
}
