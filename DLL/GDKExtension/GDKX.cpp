#include "GDKX.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <windows.h>
#include <XGameRuntime.h>

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
	XGameRuntimeInitialize();
	XUM::Init();
}

YYEXPORT
void gdk_update(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	XUM::Update();
}

YYEXPORT
void gdk_quit(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	XUM::Quit();
	XGameRuntimeUninitialize();
}

YYEXPORT
void F_XboxOneGetUserCount(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_REAL;
	Result.val = 0;

#ifdef _GAMING_XBOX
	int numusers = 0;
	XUM::GetUsers(numusers);

	Result.val = numusers;
#else

#if defined XBOX_SERVICES	
	XUM_LOCK_MUTEX
		//auto users = Windows::Xbox::System::User::Users;
		auto users = XUM::GetUsers();

	int size = users->Size;

	Result.val = size;
#endif

#endif
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

#ifdef _GAMING_XBOX
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
#else

#if defined XBOX_SERVICES
	XUM_LOCK_MUTEX
		//auto users = Windows::Xbox::System::User::Users;
		auto users = XUM::GetUsers();
	int size = users->Size;

	int index = YYGetInt32(arg, 0);

	if ((index < 0) || (index >= size))
	{
		rel_csol.Output("xboxone_get_user() - index %d out of range\n", index);
		return;
	}

	// Hmmm - hate packing a 64bit value into a variable that is only 64bits on certain platforms (it works as these functions are all only on Xbox One) :/
	// Unfortunately can't use the proper 64bit value type as the user needs to compare against a value to check for success, and this literal will be interpreted as a double (and currently the types on each side of a comparison need to match)
	uint64 id = users->GetAt(index)->XboxUserIdInt;
	Result.v64 = id;
#endif

#endif
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
#ifdef _GAMING_XBOX
	XUM_LOCK_MUTEX;
	XUserLocalId user = XUM::GetActivatingUser();
	XUMuser* xuser = XUM::GetUserFromLocalId(user);
	if (xuser == NULL)
		return;

	Result.v64 = xuser->XboxUserIdInt;
#else

#if defined XBOX_SERVICES	
	auto user = XUM::GetActivatingUser();
	if (user == nullptr)
	{
		return;
	}

	uint64 val = stringtoui64(user->XboxUserId);
	Result.v64 = val;
#endif

#endif
}

YYEXPORT
void F_XboxOneAgeGroupForUser(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_REAL;
	Result.val = -1;


#if defined XBOX_SERVICES /* XDK */
	XUM_LOCK_MUTEX
		//auto users = Windows::Xbox::System::User::Users;
		auto users = XUM::GetUsers();
	int size = users->Size;

	uint64 id = (uint64)YYGetInt64(arg, 0); // don't do any rounding on this

	for (int i = 0; i < size; i++)
	{
		auto user = users->GetAt(i);
		if (user->XboxUserIdInt == id)
		{
#ifdef WIN_UAP
			Result.val = 0.0;		// agegroup_unknown
#else
			Result.val = (double)(user->AgeGroup);
#endif
			return;
		}
	}
#elif defined _GAMING_XBOX /* GDK */
	uint64 user_id = (uint64)YYGetInt64(arg, 0);

	XUM_LOCK_MUTEX
	XUMuser* user = XUM::GetUserFromId(user_id);

	if (user != NULL)
	{
		Result.val = (double)(user->AgeGroup);
		return;
	}
	else {
		Result.val = 0.0;		// agegroup_unknown
	}
#endif

	DebugConsoleOutput("xboxone_agegroup_for_user() - user not found", false);
}

YYEXPORT
void F_XboxOnePadForUser(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_REAL;
	Result.val = -1;

	int padindex = YYGetInt32(arg, 1);

#ifdef _GAMING_XBOX
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
#else

#if defined XBOX_SERVICES
	XUM_LOCK_MUTEX
		//auto users = Windows::Xbox::System::User::Users;
		auto users = XUM::GetUsers();
	int size = users->Size;

	uint64 id = (uint64)YYGetInt64(arg, 0);

	for (int i = 0; i < size; i++)
	{
		auto user = users->GetAt(i);
		if (user->XboxUserIdInt == id)
		{
#ifndef WIN_UAP
			if (user->Controllers != nullptr)
			{
				int numcontrollers = user->Controllers->Size;
				int currpadindex = 0;

				// Get first valid gamepad			
				for (int j = 0; j < numcontrollers; j++)
				{
					auto controller = user->Controllers->GetAt(j);
					//if (controller->Type == "IGamepad")
					{
						uint64 cont_id = controller->Id;

						// Find controller in our list					
#define DEFAULT_GAMEPAD_COUNT	(8)
						extern uint64 g_padIds[DEFAULT_GAMEPAD_COUNT];
						auto pads = Windows::Xbox::Input::Gamepad::Gamepads;
						int padsize = pads->Size;
						int numgmpads = GMGamePad::GamePadCount();
						int maxpads = yymin(padsize, numgmpads);

						for (int k = 0; k < maxpads; k++)
						{
							// There's a one-to-one mapping of controllers to GMGamePad objects
							if (g_padIds[k] == cont_id)
							{
								if (currpadindex == padindex)
								{
									Result.val = k;
									return;	// found one
								}

								currpadindex++;		// step through valid pads
								break;
							}
						}
					}
				}

				// No valid pad found
				return;
			}
			else
#endif			
			{
				// No pads found
				return;
			}
		}
	}
#endif

#endif // !_GAMING_XBOX
	// No valid user found
	DebugConsoleOutput("xboxone_pad_for_user() - user not found", false);
	return;
}

int g_findSessionTimeout = 60;

void GUIDtoString(GUID* _guid, char* _outstring)
{
	if (_guid == NULL)
		return;

	if (_outstring == NULL)
		return;

	// See https://stackoverflow.com/questions/1672677/print-a-guid-variable/1672698
	//sprintf(_outstring, "%08lX-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX",
	sprintf(_outstring, "%08lx-%04hx-%04hx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx",
		_guid->Data1, _guid->Data2, _guid->Data3,
		_guid->Data4[0], _guid->Data4[1], _guid->Data4[2], _guid->Data4[3],
		_guid->Data4[4], _guid->Data4[5], _guid->Data4[6], _guid->Data4[7]);
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
