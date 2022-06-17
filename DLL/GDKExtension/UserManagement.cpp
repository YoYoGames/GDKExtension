//
// Copyright (C) 2020 Opera Norway AS. All rights reserved.
//
// This file is an original work developed by Opera.
//

#include "GDKX.h"
#include "UserManagement.h"
#include "stdlib.h"
#include <assert.h>
#include <xsapi-c/services_c.h>
#include <inttypes.h>
#include <XGameRuntime.h>

#include "PlayFabPartyManagement.h"
#include "SessionManagement.h"

#if !defined(WIN_UAP) && !defined(NO_SECURE_CONNECTION) && YY_CHAT
#include "Multiplayer/GameChat2IntegrationLayer.h"
#endif

extern unsigned int CHashMapCalculateHash(uint64_t _k);
extern bool CHashMapCompareKeys(uint64_t _k, uint64_t _k2);

// extern void CreateAsynEventWithDSMap(int dsmapindex, int event_index);
extern int GetGamepadIndex(const APP_LOCAL_DEVICE_ID* _id);

extern char* g_XboxSCID;

RefCountedGameSaveProvider* g_GameSaveProviders = NULL;

RefCountedGameSaveProvider* AllocGameSaveProvider()
{
	XUM_LOCK_MUTEX
	RefCountedGameSaveProvider* newgsp = new RefCountedGameSaveProvider();
	newgsp->provider = NULL;
	newgsp->userlocalid = XUserNullUserLocalId;
	newgsp->refcount = 1;

	newgsp->pNext = g_GameSaveProviders;
	g_GameSaveProviders = newgsp;

	return newgsp;
}

int IncRefGameSaveProvider(RefCountedGameSaveProvider* _gsp)
{
	XUM_LOCK_MUTEX;
	_gsp->refcount++;

	return _gsp->refcount;
}

int DecRefGameSaveProvider(RefCountedGameSaveProvider* _gsp)
{
	XUM_LOCK_MUTEX;
	_gsp->refcount--;
	int refcountret = _gsp->refcount;

	if (_gsp->refcount <= 0)
	{
		// Remove from list and delete
		RefCountedGameSaveProvider** ppGSP = &g_GameSaveProviders;
		while ((*ppGSP) != NULL)
		{
			if ((*ppGSP) == _gsp)
			{
				*ppGSP = _gsp->pNext;
				break;
			}

			ppGSP = &((*ppGSP)->pNext);
		}

		if (_gsp->provider != NULL)
		{
			XGameSaveCloseProvider(_gsp->provider);
		}
		delete _gsp;
	}

	return refcountret;
}

//using namespace Windows::Xbox::System;
//using namespace Windows::Foundation;
//using namespace Windows::Xbox::Input;
//using namespace Windows::Xbox::Storage;
//using namespace Concurrency;

//extern uint64_t stringtoui64(Platform::String^ _string, char* _pTempBuffToUse = NULL);

std::vector<XUMuser*> XUM::cachedUsers;
XUserLocalId XUM::activatingUser = XUserNullUserLocalId;
XUserLocalId XUM::saveDataUser = XUserNullUserLocalId;
HYYMUTEX XUM::mutex = NULL;
int XUM::currRequestID = 0;
XTaskQueueHandle XUM::m_taskQueue = NULL;
bool XUM::m_userAddInProgress = false;
XTaskQueueRegistrationToken XUM::m_userChangeEventCallbackToken = { 0 };
XTaskQueueRegistrationToken XUM::m_userDeviceAssociationChangedCallbackToken = { 0 };

XUMuser::XUMuser()
{
	_XboxLiveContext = NULL;

	MultiSubRefCount = 0;
	MultiplayerSubscriptionsEnabled = false;

	strcpy(DisplayName, "");
	GamerScore = 0;
	
	ProfileLoaded = false;
	ProfileLoading = false;

	Storage = NULL;

	SessionChangeToken = -1;			// no idea what an invalid token ID looks like (zero documentation on it)
	SubscriptionsLostToken = -1;

	// PlayFab Party stuff
	playfabLocalChatUser = NULL;
	playfabLocalChatControl = NULL;
	playfabLocalUser = NULL;
	playfabState = XUMuserPlayFabState::logged_out;
	playfabTokenExpiryTime = 0;
}

XUMuser::~XUMuser()
{
	if (Storage != NULL)
	{
		DecRefGameSaveProvider(Storage);
		//XGameSaveCloseProvider(Storage);
	}

	if (_XboxLiveContext != NULL)
	{
		XblContextCloseHandle(_XboxLiveContext);
	}

	XUserCloseHandle(user);
}

RefCountedGameSaveProvider* XUMuser::GetStorage()
{
	// Probably don't need the lock on these accessors - I'm pretty sure these operations are atomic
	XUM_LOCK_MUTEX
	return Storage;
}

unsigned int XUMuser::GetStorageStatus()
{
	XUM_LOCK_MUTEX
	return StorageStatus;
}

int	XUMuser::GetStorageError()
{
	XUM_LOCK_MUTEX
	return StorageError;
}

void XUMuser::SetStorage(RefCountedGameSaveProvider* _storage)
{
	XUM_LOCK_MUTEX
	Storage = _storage;
}

void XUMuser::SetStorageStatus(unsigned int _storageStatus)
{
	XUM_LOCK_MUTEX
	StorageStatus = _storageStatus;
}

void XUMuser::SetStorageError(int _storageError)
{
	XUM_LOCK_MUTEX
	StorageError = _storageError;
}
//extern volatile bool g_LiveConnection;


void XUMuser::SetCurrentXboxLiveContext(XblContextHandle _context)
{
	_XboxLiveContext = _context;
}

XblContextHandle XUMuser::GetCurrentXboxLiveContext()
{
	return _XboxLiveContext;
}

XblContextHandle XUMuser::GetXboxLiveContext()
{

	if (_XboxLiveContext == NULL)
	{
		if(!g_LiveConnection)
		{
			DebugConsoleOutput("GetXboxLiveContext failing due to lack of connection");
			return nullptr;
		}
			
		HRESULT hr = XblContextCreateHandle(user, &_XboxLiveContext);
		if (!SUCCEEDED(hr))
		{
			DebugConsoleOutput("Couldn't create xbl context (HRESULT 0x%08X)\n", (unsigned)(hr));			
			return NULL;
		}
	}	

	return _XboxLiveContext;
}

void XUMuser::SetUserChatPermissions(uint64_t _user_id, uint64_t _permissions)
{
	userChatPermissions[_user_id] = _permissions;		// replace any existing entry for this user
}

void XUMuser::RemoveUserChatPermissions(uint64_t _user_id)
{
	userChatPermissions.erase(_user_id);
}

uint64_t XUMuser::GetUserChatPermissions(uint64_t _user_id)
{
	auto permissions = userChatPermissions.find(_user_id);
	if (permissions == userChatPermissions.end())
		return USER_CHAT_PERMISSIONS_NONESTORED;
	else
		return permissions->second;
}

/* Why the hell can't you generate this yourself, MSVC?! */
bool operator==(const APP_LOCAL_DEVICE_ID& rhs, const APP_LOCAL_DEVICE_ID& lhs)
{
	return memcmp(&rhs, &lhs, sizeof(APP_LOCAL_DEVICE_ID)) == 0;
}

int XUMuser::AssociateController(const APP_LOCAL_DEVICE_ID* _controller)
{
	// Not sure what this is trying to do on PC
	return -1;
	if (_controller == NULL)
		return -1;

	// Check to see if we already have this controller in the list
	for(auto i = Controllers.begin(); i != Controllers.end(); ++i)
	{
		if (*i == *_controller)
		{
			// We already have this controller
			return -1;
		}
		//if (memcmp(&(*i), _controller, sizeof(APP_LOCAL_DEVICE_ID)) == 0)
		//	break;
	}

	Controllers.push_back(*_controller);
	// ControllerIndices.push_back(GetGamepadIndex(_controller));

	//assert(Controllers.size() == ControllerIndices.size());

	return ControllerIndices.back();
}

int XUMuser::DisassociateController(const APP_LOCAL_DEVICE_ID* _controller)
{
	if (_controller == NULL)
		return -1;

	// Check to see if we have this controller in the list
	auto controller_i = std::find_if(Controllers.begin(), Controllers.end(),
		[&](const APP_LOCAL_DEVICE_ID& controller) { return controller == *_controller; });

	if (controller_i == Controllers.end())
	{
		// Can't find this controller
		return -1;
	}

	int padIndex = (int)std::distance(Controllers.begin(), controller_i);

	Controllers.erase(controller_i);

	return padIndex;
}

int XUMuser::EnableMultiplayerSubscriptions()
{
	XblContextHandle context = GetXboxLiveContext();

	if (context != NULL)
	{
		if (MultiSubRefCount == 0)
		{
			// Spin while we wait for previous shutdown to complete
			// We could do this asynchronously but it would complicate the code quite a bit and it should be a rare occurance
			//while(MultiplayerSubscriptionsEnabled == true);		// hmm, could pump the message loop here

			if (MultiplayerSubscriptionsEnabled == true)
			{
				// just bail if subscriptions are already enabled
				DebugConsoleOutput("Attempting to enable multiplayer subscriptions but they're already enabled\n");
				return -1;
			}

			SessionChangeToken = XblMultiplayerAddSessionChangedHandler(context, &XUMuser::OnSessionChanged, (void*)(this->XboxUserIdInt));
			SubscriptionsLostToken = XblMultiplayerAddSubscriptionLostHandler(context, &XUMuser::OnSubscriptionsLost, (void*)(this->XboxUserIdInt));

			HRESULT res = XblMultiplayerSetSubscriptionsEnabled(context, true);
			if (FAILED(res))
			{
				XblMultiplayerRemoveSessionChangedHandler(context, SessionChangeToken);
				XblMultiplayerRemoveSubscriptionLostHandler(context, SubscriptionsLostToken);

				DebugConsoleOutput("Couldn't enable multiplayer subscriptions (XblMultiplayerSetSubscriptionsEnabled failed with 0x%08x)\n", res);
				return -1;
			}

			if (!PlayFabPartyManager::Init())
			{
				XblMultiplayerRemoveSessionChangedHandler(context, SessionChangeToken);
				XblMultiplayerRemoveSubscriptionLostHandler(context, SubscriptionsLostToken);

				return -1;
			}

			int requestID = XUM::GetNextRequestID();
			if (PlayFabPartyManager::SetupLocalUser(XboxUserIdInt, requestID) < 0)
			{
				XblMultiplayerRemoveSessionChangedHandler(context, SessionChangeToken);
				XblMultiplayerRemoveSubscriptionLostHandler(context, SubscriptionsLostToken);

				return -1;
			}

			MultiplayerSubscriptionsEnabled = true;
		}

		MultiSubRefCount++;
	}

	return -1;
}

int XUMuser::DisableMultiplayerSubscriptions()
{
	XblContextHandle context = GetXboxLiveContext();

	if (context != NULL)
	{
		if (MultiSubRefCount > 0)
		{
			MultiSubRefCount--;

			if (MultiSubRefCount == 0)
			{
				if (MultiplayerSubscriptionsEnabled == false)		// Hmmm the logic for this is complicated in the XDK version, so need to double-check
				{
					// just bail if subscriptions are already enabled
					DebugConsoleOutput("Attempting to disable multiplayer subscriptions but they're already disabled\n");
					return 1;
				}

				XblMultiplayerRemoveSessionChangedHandler(context, SessionChangeToken);
				XblMultiplayerRemoveSubscriptionLostHandler(context, SubscriptionsLostToken);

				HRESULT res = XblMultiplayerSetSubscriptionsEnabled(context, false);
				if (FAILED(res))
				{
					DebugConsoleOutput("Couldn't disable multiplayer subscriptions (XblMultiplayerSetSubscriptionsEnabled failed with 0x%08x)\n", res);
				}

				PlayFabPartyManager::CleanupLocalUser(XboxUserIdInt);
				PlayFabPartyManager::Shutdown();

				MultiplayerSubscriptionsEnabled = false;

				return 1;
			}
		}
	}

	return -1;
}

void XUMuser::RefreshPlayFabPartyLogin()
{
	if (AreMultiplayerSubscriptionsEnabled())
	{
		if (PlayFabPartyManager::ShouldRefreshLocalUserLogin(XboxUserIdInt))
		{
			int requestID = XUM::GetNextRequestID();
			if (PlayFabPartyManager::RefreshLocalUserLogin(XboxUserIdInt, requestID) < 0)
			{
				extern void StopMultiplayerForUser(XUMuser * user);
				StopMultiplayerForUser(this);				
			}
		}
	}
}

void XUMuser::SetupStorage()
{
	XUM::LockMutex();
	if (StorageStatus == CSSTATUS_INVALID)
	{
		StorageStatus = CSSTATUS_SETTINGUP;
		XUM::UnlockMutex();

		struct InitContext
		{
			XUserLocalId self;
			XAsyncBlock async;
		};

		InitContext* ctx = new InitContext{};
		if (ctx)
		{
			ctx->self = LocalId;
			ctx->async.context = ctx;
			ctx->async.callback = [](XAsyncBlock* async)
			{
				XUM::LockMutex();
				InitContext* ctx = reinterpret_cast<InitContext*>(async->context);
				XUMuser* self = XUM::GetUserFromLocalId(ctx->self);
				if (self != NULL)	// it's possible the user might have logged out as this was happening so they may no longer exist
				{
					XGameSaveProviderHandle provider = nullptr;
					HRESULT hr = XGameSaveInitializeProviderResult(async, &provider);
					if (SUCCEEDED(hr))
					{
						RefCountedGameSaveProvider* gsp = AllocGameSaveProvider();
						gsp->provider = provider;
						gsp->userlocalid = ctx->self;
						self->SetStorage(gsp);
						self->SetStorageStatus(CSSTATUS_AVAILABLE);
						self->SetStorageError(S_OK);
					}
					else
					{
						self->SetStorage(NULL);
						self->SetStorageStatus(CSSTATUS_INVALID);
						self->SetStorageError(hr);
					}
				}
				XUM::UnlockMutex();
				delete ctx;
			};
			
			HRESULT hr = XGameSaveInitializeProviderAsync(user, g_XboxSCID, false, &ctx->async);
			if (FAILED(hr))
			{
				XUM::LockMutex();
				SetStorage(NULL);
				SetStorageStatus(CSSTATUS_INVALID);
				SetStorageError(hr);
				XUM::UnlockMutex();
			}
		}
		else
		{
			XUM::LockMutex();
			StorageStatus = CSSTATUS_INVALID;
			XUM::UnlockMutex();
		}
	}
	else
	{
		XUM::UnlockMutex();
	}
}

void
XUMuser::OnSessionChanged(
	void* context,
	XblMultiplayerSessionChangeEventArgs args
)
{
	uint64_t user_id = (uint64_t)context;

	// Call static XSM::OnSessionChanged function
	XSM::OnSessionChanged(user_id, args);
}

void
XUMuser::OnSubscriptionsLost(
	void* context
)
{
#if 0
	// Previously, when turning off an Xbox which had Instant-On enabled, users would get signed out which would cause us to ultimately call StopMultiplayerForUser()
	// which would remove the user from all sessions they were part of and shut down game chat
	// Now, for reasons I don't quite understand, this behaviour has changed (maybe a firmware update)
	// Now the users don't get signed out, but *do* still receive this OnSubscriptionsLost() callback	
	// So I'll double check that everything is shut down and cleaned up here
	// Note that if you launch the game from Visual Studio you *still* get sign-out callbacks so unfortunately the behaviour is different from when launching from an installed package
	extern void CleanupMultiplayerForUser(XUMuser^user);
	CleanupMultiplayerForUser(this);

	MultiplayerSubscriptionsEnabled = false;

	// Trigger async event to inform the user that multiplayer subscriptions have been lost
	{
		//DS_LOCK_MUTEX;		// we don't need to do this here as it's done in CreateDsMap, GetDsMap and CreateAsynEventWithDSMap - in fact it can cause a deadlock due to a nested HTTP_LOCK_MUTEX inside CreateAsynEventWithDSMap

		int dsMapIndex = CreateDsMap(1,
			"event_type", (double) 0.0, "user matchmaking stopped");

		CDS_Map* pTheMap = GetDsMap(dsMapIndex);
		RValue key = { 0 };
		RValue value = { 0 };
		YYSetString(&key, "user");
		value.kind = VALUE_INT64;
		value.v64 = (XboxUserIdInt);
		pTheMap->Add(&key, &value);
		FREE_RValue(&key);
		FREE_RValue(&value);

		CreateAsynEventWithDSMap(dsMapIndex, EVENT_OTHER_SYSTEM_EVENT);
	}
#endif
}

void XUM::LockMutex()
{
	YYMutexLock(mutex);
}

void XUM::UnlockMutex()
{
	YYMutexUnlock(mutex);
}

int XUM::GetNextRequestID()
{
	int id = currRequestID;
	currRequestID++;

	if (currRequestID < 0)
	{
		currRequestID = 0;
	}

	return id;
}

void XUM::Init()
{
	mutex = YYMutexCreate("XUM");

	currRequestID = 0;

	// Create a task queue that will process in the background on system threads and fire callbacks on a thread we choose in a serialized order
	HRESULT hResult = XTaskQueueCreate(XTaskQueueDispatchMode::ThreadPool, XTaskQueueDispatchMode::Manual, &m_taskQueue);
	if (FAILED(hResult)) {
		DebugConsoleOutput("XUM: XTaskQueueCreate failed (HRESULT 0x%08X)\n", (unsigned)(hResult));
		return;
	} // end if

	// Register for any change events for user.
	// This allows us to know when users sign out, changes to gamertags, and more.
	XUserRegisterForChangeEvent(
		m_taskQueue,
		NULL,
		&UserChangeEventCallback,
		&m_userChangeEventCallbackToken
	);

	// Registers for any change to device association so that the application can keep
	// up-to-date information about users and their associated devices.
	XUserRegisterForDeviceAssociationChanged(
		m_taskQueue,
		NULL,
		&UserDeviceAssociationChangedCallback,
		&m_userDeviceAssociationChangedCallbackToken
	);
	
	//RefreshCachedUsers();	// set up initial list
	activatingUser = XUserNullUserLocalId;
	saveDataUser = XUserNullUserLocalId;

	// RK :: Gersh advises allowing UI is the best default.... 
	AddUser(XUserAddOptions::AddDefaultUserAllowingUI, false);	// set up default user
}

void XUM::Quit()
{
	if (m_taskQueue != NULL)
	{
		XUserUnregisterForDeviceAssociationChanged(m_userDeviceAssociationChangedCallbackToken, false);
		XUserUnregisterForChangeEvent(m_userChangeEventCallbackToken, false);

		XTaskQueueCloseHandle(m_taskQueue);
		m_taskQueue = NULL;
	}
}


void XUM::Update()
{
	static bool firstframe = true;
	if (firstframe)
	{
		firstframe = false;		// do this so we have a chance to populate our controller list before handling device associations
		return;
	}

	for(auto i = cachedUsers.begin(); i != cachedUsers.end(); ++i)
	{
		XUMuser* xumuser = *i;
		if (xumuser != NULL)
		{
			xumuser->RefreshPlayFabPartyLogin();
		}
	}

	// Handle callbacks in the main thread to ensure thread safety
	while (m_taskQueue != NULL && XTaskQueueDispatch(m_taskQueue, XTaskQueuePort::Completion, 0))
	{
	}
}

//extern int g_HTTP_ID;

int XUM::AddUser(XUserAddOptions _options, bool _fromManualAccountPicker)
{
	if (m_userAddInProgress)
		return XUMADDUSER_IN_PROGRESS;

	struct AddUserContext
	{
		bool fromManualAccountPicker;
		int id;

		XAsyncBlock async;
	};

	AddUserContext* ctx = new AddUserContext();

	ctx->fromManualAccountPicker = _fromManualAccountPicker;
	ctx->id = g_HTTP_ID++;

	// Setup async block and function
	ctx->async.queue = m_taskQueue;
	ctx->async.context = ctx;
	ctx->async.callback = [](XAsyncBlock* asyncBlock)
	{
		AddUserContext* ctx = (AddUserContext*)(asyncBlock->context);

		XUM_LOCK_MUTEX;		// this should be run on the main thread anyway, but just in case

		XUserHandle newUser = NULL;
		uint64_t xboxuserIDint = 0;
		HRESULT result = XUserAddResult(asyncBlock, &newUser);

		if (SUCCEEDED(result))
		{
			DebugConsoleOutput("!!!!!!!!! Sign-in succeeded! (HRESULT 0x%08X)\n", (unsigned)(result));

			// See if we already have this user
			auto i = std::find_if(cachedUsers.begin(), cachedUsers.end(),
				[&](const XUMuser* cachedUser)
			{
				DebugConsoleOutput("!!!!!!!!! Same user\n");

				// Apparently we can get an identical handle back, or a different handle which points to the same user
				if (cachedUser != NULL)
				{
					if (cachedUser->user == newUser)
					{
						xboxuserIDint = cachedUser->XboxUserIdInt;
						return true;
					}
					else if (XUserCompare(cachedUser->user, newUser) == 0)
					{
						xboxuserIDint = cachedUser->XboxUserIdInt;
						XUserCloseHandle(newUser);		// we already have a different handle to the same user, so free the new handle
						return true;
					}
				}
				return false;
			});

			if (i == cachedUsers.end())
			{
				DebugConsoleOutput("!!!!!!!!! New user\n");
				// New user found, so add to list
				XUMuser* newXUMuser = new XUMuser;
				newXUMuser->user = newUser;
				size_t dummysize;
				XUserGetGamertag(newUser, XUserGamertagComponent::Modern, XBOX_USER_GAMERTAG_MAX_LENGTH, newXUMuser->ModernGamertag, &dummysize);
				XUserGetGamertag(newUser, XUserGamertagComponent::ModernSuffix, XBOX_USER_GAMERTAG_MAX_LENGTH, newXUMuser->GamertagSuffix, &dummysize);
				XUserGetGamertag(newUser, XUserGamertagComponent::UniqueModern, XBOX_USER_GAMERTAG_MAX_LENGTH, newXUMuser->UniqueModernGamertag, &dummysize);
				XUserGetIsGuest(newUser, &(newXUMuser->IsGuest));
				XUserState userstate;
				XUserGetState(newUser, &userstate);
				newXUMuser->IsSignedIn = (userstate == XUserState::SignedIn) ? true : false;
				XUserGetAgeGroup(newUser, &(newXUMuser->AgeGroup));
				XUserGetId(newUser, &(newXUMuser->XboxUserIdInt));
				XUserGetLocalId(newUser, &(newXUMuser->LocalId));

				newXUMuser->SetStorageStatus(CSSTATUS_INVALID);
				newXUMuser->SetStorage(nullptr);
				newXUMuser->SetupStorage();

				cachedUsers.push_back(newXUMuser);

				if (cachedUsers.size() == 1)
				{
					activatingUser = newXUMuser->LocalId;
				}

				newXUMuser->ProfileLoading = true;
				UpdateUserProfileData(newXUMuser->XboxUserIdInt, true);

				xboxuserIDint = newXUMuser->XboxUserIdInt;
			}
		}
		else
		{
			DebugConsoleOutput("Sign-in failed (HRESULT 0x%08X)\n", (unsigned)(result));

			{
				// DS_LOCK_MUTEX;			// a bit dangerous being inside a XUM_LOCK_MUTEX block - split this apart

				int dsMapIndex = CreateDsMap(2,
					"event_type", (double)(0.0),    "user sign in failed",
					"error",      (double)(result), NULL);

				CreateAsyncEventWithDSMap(dsMapIndex, EVENT_OTHER_SYSTEM_EVENT);
			}
		}

		if (ctx->fromManualAccountPicker)
		{
			// DS_LOCK_MUTEX;

			double succeeded = (xboxuserIDint != 0) ? 1.0 : 0.0;

			// Create a ds map with the info in it
			int dsMapIndex = CreateDsMap(3,
				"id",        (double)(ctx->id), NULL,
				"type",      (double)(0.0), "xboxone_accountpicker",
				"succeeded", succeeded,     NULL
			);

			// Need to add user field seperately as CreateDsMap doesn't handle VALUE_INT64 types
			DsMapAddInt64(dsMapIndex, "user", xboxuserIDint);

			CreateAsyncEventWithDSMap(dsMapIndex, EVENT_OTHER_DIALOG_ASYNC);
		}

		delete ctx;
		m_userAddInProgress = false;
	};

	HRESULT result = XUserAddAsync(_options, &(ctx->async));

	// Try to perform default user sign-in
	if (SUCCEEDED(result))
	{
		DebugConsoleOutput("XUserAddAsync Succeed (HRESULT 0x%08X)\n", (unsigned)(result));

		// Async action started
		m_userAddInProgress = true;

		if (_fromManualAccountPicker)
		{
			return ctx->id;
		}
		else
		{
			return XUMADDUSER_OK;
		}
	}
	else
	{
		DebugConsoleOutput("XUserAddAsync failed (HRESULT 0x%08X)\n", (unsigned)(result));

		// Failed, so be sure to clean async block
		delete ctx;
	}

	return XUMADDUSER_ADD_FAILED;
}

void XUM::UpdateUserProfileData(uint64_t user_id, bool _newUser)
{
	XUMuser* user = GetUserFromId(user_id);
	if (user == NULL)
	{
		DebugConsoleOutput("XUM::UpdateUserProfileData() - couldn't get user\n");
		return;
	}

	XblContextHandle xbl_ctx;

	HRESULT hr = XblContextCreateHandle(user->user, &xbl_ctx);
	if (!SUCCEEDED(hr))
	{
		DebugConsoleOutput("XUM::UpdateUserProfileData() - couldn't create xbl handle (HRESULT 0x%08X)\n", (unsigned)(hr));
		return;
	}

	struct UpdateUserProfileDataContext
	{
		uint64_t user_id;
		XblContextHandle xbl_ctx;
		bool new_user;

		UpdateUserProfileDataContext(uint64_t user_id, XblContextHandle xbl_ctx, bool _newUser) :
			user_id(user_id), xbl_ctx(xbl_ctx), new_user(_newUser) {}

		~UpdateUserProfileDataContext()
		{
			XblContextCloseHandle(xbl_ctx);
		}
	};

	UpdateUserProfileDataContext* ctx = new UpdateUserProfileDataContext(user_id, xbl_ctx, _newUser);

	XAsyncBlock* async = new XAsyncBlock;
	async->queue = m_taskQueue;
	async->context = ctx;
	async->callback = [](XAsyncBlock* async)
	{
		UpdateUserProfileDataContext* ctx = (UpdateUserProfileDataContext*)(async->context);

		bool profile_loaded = false;

		{
			XUM_LOCK_MUTEX

			XUMuser* user = GetUserFromId(ctx->user_id);
			if (user == NULL)
			{
				/* User went away before our XblProfileGetUserProfileAsync request completed. */

				delete ctx;
				delete async;

				return;
			}

			std::unique_ptr<XblUserProfile> profile = std::make_unique<XblUserProfile>();

			HRESULT hr = XblProfileGetUserProfileResult(async, profile.get());
			if (SUCCEEDED(hr))
			{
				static_assert((sizeof(user->DisplayName) >= sizeof(profile->gameDisplayName)), "XUMuser::DisplayName buffer is large enough");
				strcpy(user->DisplayName, profile->gameDisplayName);

				user->GamerScore = strtoul(profile->gamerscore, NULL, 10);

				user->ProfileLoaded = true;
				profile_loaded = true;
			}
			else {
				DebugConsoleOutput("XUM::UpdateUserProfileData() - XblProfileGetUserProfileAsync failed (HRESULT 0x%08X)\n", (unsigned)(hr));
			}
		}

		// always report sign-in for new users regardless of whether we managed to load the profile (we need to do this for offline scenarios)
		if (ctx->new_user)
		{
			/* First fetch of profile data since user was cached. Fire the user sign in event.
			 *
			 * TODO: We should maybe do a refresh periodically from now to renew DisplayName and
			 * GamerScore. There doesn't seem to be a profile update event we can react to.
			*/

			DebugConsoleOutput("User %" PRIu64 " signed in\n", ctx->user_id);

			// DS_LOCK_MUTEX;

			int dsMapIndex = CreateDsMap(1,
				"event_type", (double)0.0, "user signed in");

			DsMapAddInt64(dsMapIndex, "user", ctx->user_id);

			CreateAsyncEventWithDSMap(dsMapIndex, EVENT_OTHER_SYSTEM_EVENT);
		}

		if (profile_loaded)
		{
			DebugConsoleOutput("User %" PRIu64 " profile loaded\n", ctx->user_id);

			// DS_LOCK_MUTEX;

			int dsMapIndex = CreateDsMap(1,
				"event_type", (double)0.0, "user profile updated");

			DsMapAddInt64(dsMapIndex, "user", ctx->user_id);

			CreateAsyncEventWithDSMap(dsMapIndex, EVENT_OTHER_SYSTEM_EVENT);
		}
		
		delete ctx;
		delete async;
	};

	hr = XblProfileGetUserProfileAsync(ctx->xbl_ctx, user_id, async);
	if (!SUCCEEDED(hr))
	{
		DebugConsoleOutput("XUM::UpdateUserProfileData() - XblProfileGetUserProfileAsync failed (HRESULT 0x%08X)\n", (unsigned)(hr));
	
		delete ctx;
		delete async;
	}
}

XUMuser** XUM::GetUsers(int &out_numusers)
{
	XUM_LOCK_MUTEX;
	out_numusers = (int)cachedUsers.size();
	return cachedUsers.data();
}

XUserLocalId XUM::GetActivatingUser()
{
	XUM_LOCK_MUTEX
	return activatingUser;
}

void XUM::SetSaveDataUser(XUserLocalId _user)
{
	saveDataUser = _user;
}

XUserLocalId XUM::GetSaveDataUser()
{
	return saveDataUser;
}

#if 0
XUMuser^ XUM::GetUserFromSessionMember(Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionMember^ _member)
{
	if (_member == nullptr)
		return nullptr;

	// Extract xbox user id from session member
	uint64_t xboxuserIDint = stringtoui64(_member->XboxUserId);

	return GetUserFromId(xboxuserIDint);
}
#endif

XUMuser* XUM::GetUserFromId(uint64_t xboxuserIDint)
{
	// Now see if we have a matching user in our list	
	auto user_i = std::find_if(cachedUsers.begin(), cachedUsers.end(),
		[&](const XUMuser* cachedUser) { return cachedUser != NULL && cachedUser->XboxUserIdInt == xboxuserIDint; });

	return user_i != cachedUsers.end()
		? *user_i
		: NULL;
}

XUMuser* XUM::GetUserFromLocalId(XUserLocalId _id)
{
	// See if we have a matching user in our list	
	auto user_i = std::find_if(cachedUsers.begin(), cachedUsers.end(),
		[&](const XUMuser* cachedUser) { return cachedUser != NULL && cachedUser->LocalId.value == _id.value; });

	return user_i != cachedUsers.end()
		? *user_i
		: NULL;
}

void XUM::RemoveUserChatPermissions(uint64_t _user_id)
{
	for (auto user_i = cachedUsers.begin(); user_i != cachedUsers.end(); ++user_i)
	{		
		XUMuser* cachedUser = *user_i;

		if (cachedUser != NULL)
		{
			cachedUser->RemoveUserChatPermissions(_user_id);
		}
	}
}

void XUM::UpdateUserProfiles(bool _onlyLoadFailed)
{
	XUM_LOCK_MUTEX

		int numCachedUsers = cachedUsers.size();
		for (int i = 0; i < numCachedUsers; i++)
		{
			if (cachedUsers[i] != NULL)
			{
				XUMuser* user = cachedUsers[i];
				if (user->ProfileLoading)
					continue;	// if we're already trying to load this profile then don't try again just now

				if (_onlyLoadFailed && (user->ProfileLoaded))
					continue;	// if we're only trying to load the profiles which failed, bail on this one that has previously succeeded

				UpdateUserProfileData(user->XboxUserIdInt, false);
			}
		}
}

void XUM::RefreshCachedUsers(bool PreserveOldUsers)
{
#if 0
	XUM_LOCK_MUTEX
	Platform::Collections::Vector<XUMuser^> ^oldUsers = ref new Platform::Collections::Vector<XUMuser^>();

	if (cachedUsers)
	{
		unsigned int size = cachedUsers->Size;
		for(int i = 0; i < size; i++)
		{
			oldUsers->Append(cachedUsers->GetAt(i));
		}
	}

	cachedUsers = ref new Platform::Collections::Vector<XUMuser^>();		// this'll delete any data already held

	auto users = Windows::Xbox::System::User::Users;
	unsigned int size = users->Size;

	for(int i = 0; i < size; i++)
	{
		Windows::Xbox::System::User^ user = users->GetAt(i);

		//If we had this user in the old list copy him into this list as he may have callbacks and allsorts already setup

		bool addrequired = true;

		int osize = oldUsers->Size;
		for (int j = 0; j < osize; j++)
		{
			if (user->XboxUserId == oldUsers->GetAt(j)->XboxUserId && PreserveOldUsers)
			{
				//Copy him in
				cachedUsers->Append(oldUsers->GetAt(j));
				addrequired = false;
			}


		}

		if (addrequired)
		{
			XUMuser^ newuser = ref new XUMuser();
			newuser->user = user;
			newuser->Id = user->Id;
			newuser->GameDisplayName = user->DisplayInfo->GameDisplayName;
			newuser->AppDisplayName = user->DisplayInfo->ApplicationDisplayName;
			newuser->Controllers = user->Controllers;
			newuser->IsGuest = user->IsGuest;
			newuser->IsSignedIn = user->IsSignedIn;
			newuser->Location = user->Location;
			newuser->AgeGroup = user->DisplayInfo->AgeGroup;
			newuser->GamerScore = user->DisplayInfo->GamerScore;
			newuser->Reputation = user->DisplayInfo->Reputation;
			newuser->XboxUserId = user->XboxUserId;
			uint64_t xboxuserIDint = stringtoui64(newuser->XboxUserId);
			newuser->XboxUserIdInt = xboxuserIDint;


			newuser->SetStorageStatus(CSSTATUS_INVALID);
			newuser->SetStorage(nullptr);
			newuser->SetupStorage();

			cachedUsers->Append(newuser);
		}
	}

	size = oldUsers->Size;
	for(int i = 0; i < size; i++)
	{
		// look through cached users
		int cachedSize = cachedUsers->Size;
		bool signedOut = true;

		for (int j = 0; j < cachedSize; j++)
		{
			// do stuff
			if (cachedUsers->GetAt(j)->XboxUserIdInt== oldUsers->GetAt(i)->XboxUserIdInt)
			{
				signedOut = false;
				break;
			}
		}

		if (signedOut)
		{
			uint64_t xboxuserIDint = (oldUsers->GetAt(i)->XboxUserIdInt);

			{
				DS_LOCK_MUTEX;

				int dsMapIndex = CreateDsMap(1,
					"event_type", (double) 0.0, "user signed out");

				CDS_Map* pTheMap = GetDsMap(dsMapIndex);
				RValue key = { 0 };
				RValue value = { 0 };
				YYSetString(&key, "user");
				value.kind = VALUE_INT64;
				value.v64 = (xboxuserIDint);
				pTheMap->Add(&key, &value);
				FREE_RValue(&key);
				FREE_RValue(&value);

				CreateAsynEventWithDSMap(dsMapIndex, EVENT_OTHER_SYSTEM_EVENT);
			}

			//If user attached to subs nuke them here
			extern void StopMultiplayerForUser(XUMuser^user);
			StopMultiplayerForUser(oldUsers->GetAt(i));

		}
	}

	int cachedSize = cachedUsers->Size;

	for (int i = 0; i < cachedSize; i++)
	{
		bool signedIn = true;
		size = oldUsers->Size;

		for(int j = 0; j < size; j++)
		{
			if (cachedUsers->GetAt(i)->XboxUserIdInt == oldUsers->GetAt(j)->XboxUserIdInt)
			{
				signedIn = false;
				break;
			}
		}

		if (signedIn)
		{
			uint64_t xboxuserIDint = stringtoui64(cachedUsers->GetAt(i)->XboxUserId);

			int dsMapIndex = CreateDsMap( 1,	
				"event_type", (double) 0.0, "user signed in");

			CDS_Map* pTheMap = GetDsMap(dsMapIndex);
			RValue key = {0};
			RValue value = {0};
			YYSetString( &key, "user" );
			value.kind = VALUE_INT64;
			value.v64 = (xboxuserIDint);
			pTheMap->Add(&key, &value);
			FREE_RValue(&key);
			FREE_RValue(&value);

			CreateAsynEventWithDSMap(dsMapIndex, EVENT_OTHER_SYSTEM_EVENT);
		}
	}

	ApplicationView::OnLicenseChanged();
#endif
}


void CALLBACK XUM::UserChangeEventCallback(
	_In_opt_ void* context,
	_In_ XUserLocalId userLocalId,
	_In_ XUserChangeEvent event
)
{
	// This is not triggered by a user signing in unless we've retained an XUserHandle to a user which has signed out, then signed back in
	// But we'll remove users on sign-out so that eventuality shouldn't occur	

	XUM_LOCK_MUTEX;

	auto cachedUser_i = std::find_if(cachedUsers.begin(), cachedUsers.end(),
		[&](const XUMuser* cachedUser) { return cachedUser != NULL && cachedUser->LocalId.value == userLocalId.value; });

	if (cachedUser_i == cachedUsers.end())
	{
		// User not found, so something has gone wrong
		DebugConsoleOutput("State change detected on user %ull but we are not tracking this user\n", userLocalId.value);
	}
	else
	{
		XUMuser* cachedUser = *cachedUser_i;

		if (event == XUserChangeEvent::SignedOut)
		{
			uint64_t xboxuserIDint = (cachedUser->XboxUserIdInt);

			DebugConsoleOutput("User %ld signed out\n", xboxuserIDint);

			if(cachedUser->ProfileLoaded)
			{
				// DS_LOCK_MUTEX;

				int dsMapIndex = CreateDsMap(1,
					"event_type", (double)0.0, "user signed out");

				DsMapAddInt64(dsMapIndex, "user", xboxuserIDint);

				CreateAsyncEventWithDSMap(dsMapIndex, EVENT_OTHER_SYSTEM_EVENT);
			}

			if (activatingUser.value == cachedUser->LocalId.value)
			{
				activatingUser = XUserNullUserLocalId;
			}

			delete* cachedUser_i;
			cachedUsers.erase(cachedUser_i);
		}
	}
}

void CALLBACK XUM::UserDeviceAssociationChangedCallback(
	_In_opt_ void* context,
	_In_ const XUserDeviceAssociationChange* change
)
{
	YYASSERT(change);

	XUMuser* oldUser = NULL;
	XUMuser* newUser = NULL;

	if (memcmp(&(change->oldUser), &XUserNullUserLocalId, sizeof(XUserLocalId)) != 0)	// if not a NULL user
	{
		oldUser = GetUserFromLocalId(change->oldUser);
	}

	if (memcmp(&(change->newUser), &XUserNullUserLocalId, sizeof(XUserLocalId)) != 0)	// if not a NULL user
	{
		newUser = GetUserFromLocalId(change->newUser);
	}

	if (oldUser != NULL)
	{
		int padIndex = oldUser->DisassociateController(&(change->deviceId));
		
		if (padIndex != -1)
		{		
			// DS_LOCK_MUTEX;

			int dsMapIndex = CreateDsMap(2,
				"event_type", (double)(0.0), "user controller disassociated",
				"pad_index", (double)(padIndex), NULL);

			DsMapAddInt64(dsMapIndex, "user", oldUser->XboxUserIdInt);

			CreateAsyncEventWithDSMap(dsMapIndex, EVENT_OTHER_SYSTEM_EVENT);
		}
	}

	if (newUser != NULL)
	{
		int padIndex = newUser->AssociateController(&(change->deviceId));

		if (padIndex != -1)
		{
			// DS_LOCK_MUTEX;

			int dsMapIndex = CreateDsMap(2,
				"event_type", (double)(0.0), "user controller associated",
				"pad_index", (double)(padIndex), NULL);

			DsMapAddInt64(dsMapIndex, "user", newUser->XboxUserIdInt);

			CreateAsyncEventWithDSMap(dsMapIndex, EVENT_OTHER_SYSTEM_EVENT);
		}
	}
}