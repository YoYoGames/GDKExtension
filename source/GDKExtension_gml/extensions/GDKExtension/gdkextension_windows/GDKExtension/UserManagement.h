//
// Copyright (C) 2020 Opera Norway AS. All rights reserved.
//
// This file is an original work developed by Opera.
//

#ifndef USERMANAGEMENT_H
#define USERMANAGEMENT_H

#include "GDKX.h"

#include <memory>
#include <unordered_map>
#include <stddef.h>
#include <stdint.h>
#include <vector>
#include <xsapi-c/types_c.h>
#include <xsapi-c\services_c.h>
#include <xuser.h>
#include <XGameSave.h>

#include "Party.h"
#include "PartyXboxLive.h"

#if (_XDK_VER >= 11785)
#define NOV_XDK
#endif

//#define XBOX_USER	XUserHandle
#define XBOX_USER_GAMERTAG_MAX_LENGTH	64

struct RefCountedGameSaveProvider
{
	XGameSaveProviderHandle provider;
	XUserLocalId userlocalid;
	int refcount;

	RefCountedGameSaveProvider* pNext;	
};

int IncRefGameSaveProvider(RefCountedGameSaveProvider* _gsp);
int DecRefGameSaveProvider(RefCountedGameSaveProvider* _gsp);

enum
{
	CSSTATUS_INVALID,
	CSSTATUS_SETTINGUP,
	CSSTATUS_AVAILABLE,
};

enum
{
	XUMADDUSER_OK = 0,
	XUMADDUSER_ADD_FAILED = -1,
	XUMADDUSER_IN_PROGRESS = -2,
};

#define USER_CHAT_PERMISSIONS_NONESTORED 0

enum class XUMuserPlayFabState : uint32_t
{
	logged_out = 0,
	logging_in,
	logged_in,
	logging_out,
};

class XUMuser
{
private:
	XblContextHandle _XboxLiveContext;
public:
	XUMuser();
	~XUMuser();

	XUserHandle user;	// the actual user object

	// Cached fields (basically more quickly accessible copies of stuff stored in 'user')
	char ModernGamertag[XBOX_USER_GAMERTAG_MAX_LENGTH];
	char GamertagSuffix[XBOX_USER_GAMERTAG_MAX_LENGTH];
	char UniqueModernGamertag[XBOX_USER_GAMERTAG_MAX_LENGTH];
	// APP_LOCAL_DEVICE_ID* Controllers;
	// int* ControllerIndices;
	// int numControllers;
	std::vector<APP_LOCAL_DEVICE_ID> Controllers;
	std::vector<int> ControllerIndices;
	bool IsGuest;
	bool IsSignedIn;
	//Windows::Xbox::System::UserLocation Location;
	XUserAgeGroup AgeGroup;
	char DisplayName[XBL_DISPLAY_NAME_CHAR_SIZE];
	uint32_t GamerScore;
	//int32 Reputation;
	uint64_t XboxUserIdInt;
	XUserLocalId LocalId;
	int MultiSubRefCount;
	XblContextHandle GetXboxLiveContext();
	XblContextHandle GetCurrentXboxLiveContext();
	void SetCurrentXboxLiveContext(XblContextHandle _context);
	
	bool ProfileLoaded;
	bool ProfileLoading;

	// PlayFab Party stuff
	Party::PartyXblLocalChatUser* playfabLocalChatUser;
	Party::PartyLocalChatControl* playfabLocalChatControl;
	Party::PartyLocalUser* playfabLocalUser;
	XUMuserPlayFabState playfabState;
	time_t playfabTokenExpiryTime;

	std::unordered_map<uint64_t, uint64_t> userChatPermissions;
	void SetUserChatPermissions(uint64_t _user_id, uint64_t _permissions);
	void RemoveUserChatPermissions(uint64_t _user_id);
	uint64_t GetUserChatPermissions(uint64_t _user_id);

	int AssociateController(const APP_LOCAL_DEVICE_ID* _controller);
	int DisassociateController(const APP_LOCAL_DEVICE_ID* _controller);

	int EnableMultiplayerSubscriptions();
	int DisableMultiplayerSubscriptions();
	bool AreMultiplayerSubscriptionsEnabled() { return MultiplayerSubscriptionsEnabled; }
	void RefreshPlayFabPartyLogin();

	RefCountedGameSaveProvider* GetStorage();
	unsigned int GetStorageStatus();
	int	GetStorageError();

	void SetStorage(RefCountedGameSaveProvider* _storage);
	void SetStorageStatus(unsigned int _storageStatus);
	void SetStorageError(int _storageError);

	void SetupStorage();

	static void OnSessionChanged(
		void* context,
		XblMultiplayerSessionChangeEventArgs args
		);

	static void OnSubscriptionsLost(
		void* context
        );

private:
	// Connected storage	
	RefCountedGameSaveProvider* Storage;
	unsigned int StorageStatus;
	int StorageError;
	XblFunctionContext	SessionChangeToken;
	XblFunctionContext	SubscriptionsLostToken;

	volatile bool MultiplayerSubscriptionsEnabled;
};

class XUM
{
	friend struct XUM_AutoMutex;	
public:	

	static void Init();
	static void Quit();

	static void Update();	
	
	static int AddUser(XUserAddOptions _options, bool _fromManualAccountPicker);
	static void UpdateUserProfileData(uint64_t user_id, bool _newUser);
	static XUMuser** GetUsers(int &out_numusers);
	static XUserLocalId GetActivatingUser();
	static void SetSaveDataUser(XUserLocalId _user);
	static XUserLocalId GetSaveDataUser();
	static XUMuser* GetUserFromId(uint64_t id);
	static XUMuser* GetUserFromLocalId(XUserLocalId _id);

	static void RemoveUserChatPermissions(uint64_t _user_id);				// removes the chat permissions for this user from all cached users

	static void UpdateUserProfiles(bool _onlyLoadFailed);

	static void RefreshCachedUsers(bool PreserveOldUsers = true);

	static void LockMutex();
	static void UnlockMutex();

	static int GetNextRequestID();
	static XTaskQueueHandle GetTaskQueue() { return m_taskQueue; }
private:
	static std::vector<XUMuser*> cachedUsers;
	static XUserLocalId activatingUser;
	static XUserLocalId saveDataUser;		// the user that is currently associated with save data handling

	static HYYMUTEX mutex;

	static int currRequestID;

	// Async data
	static XTaskQueueHandle m_taskQueue;
	static bool m_userAddInProgress;	

	// Callbacks
	static XTaskQueueRegistrationToken m_userChangeEventCallbackToken;
	static XTaskQueueRegistrationToken m_userDeviceAssociationChangedCallbackToken;
	static void CALLBACK UserChangeEventCallback(
		_In_opt_ void* context,
		_In_ XUserLocalId userLocalId,
		_In_ XUserChangeEvent event
	);

	static void CALLBACK UserDeviceAssociationChangedCallback(
		_In_opt_ void* context,
		_In_ const XUserDeviceAssociationChange* change
	);
};

struct XUM_AutoMutex
{
	XUM_AutoMutex()
	{
		YYMutexLock(XUM::mutex);
	}

	~XUM_AutoMutex()
	{
		YYMutexUnlock(XUM::mutex);
	}
};

#define XUM_LOCK_MUTEX XUM_AutoMutex __xum_automutex;

#endif // USERMANAGEMENT_H