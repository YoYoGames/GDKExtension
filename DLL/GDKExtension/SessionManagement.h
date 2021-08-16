#ifndef SESSIONMANAGEMENT_H
#define SESSIONMANAGEMENT_H

#ifndef WIN_UAP
#if defined(_GAMING_XBOX) || defined(YYGDKEXTENSION)
#include <pch.h>
#include <mutex>
#include <stdint.h>
#include <unordered_map>
#include <vector>
// #include "yoyo_types.h"

//#include "Party.h"
//#include "PartyXboxLive.h"
#include "PlayFabPartyManagement.h"
#else
#include <xdk.h>
#endif
#endif

#define ENFORCE_STAR_NETWORK_TOPOLOGY
#define REPORT_SESSION_JOINED_FOR_ALL_MEMBERS

#pragma message(".............. Add XBOX_LIVE_VER=1508 to preprocessor definitions in project settings if using standalone Xbox Live SDK (replace version with appropriate value)")


#ifdef WIN_UAP
#define USE_WRITE_SESSION_ASYNC

namespace Windows
{
	namespace Xbox
	{
		namespace Services
		{
			ref class XboxLiveConfiguration
			{
			public:
				static property Platform::String^ PrimaryServiceConfigId
				{
					Platform::String^ get()
					{
						// Create and initialise an instance of "local_config_impl" which has methods for reading the contents of the "xboxservices.config" file
						// For now just return a fixed string
						if (primaryserviceconfigid == nullptr)
						{
							primaryserviceconfigid = ref new Platform::String(L"hello");
						}

						return primaryserviceconfigid;
					}
				}
			private:
				static Platform::String^ primaryserviceconfigid;
			};
		}
	}
}
#endif

#include "SecureConnectionManager.h"

#define XSM_VERBOSE_TRACE

#include "UserManagement.h"

struct xbl_session
{
	xbl_session(XblMultiplayerSessionHandle _handle) : session_handle(_handle) {}
	~xbl_session()
	{
		XblMultiplayerSessionCloseHandle(session_handle);
	}

	XblMultiplayerSessionHandle session_handle;
};

typedef std::shared_ptr<xbl_session> xbl_session_ptr;	// makes this a refcounted type

struct XSMPlayFabPayload : public PlayFabPayloadBase
{
	uint64_t user_id;
	xbl_session_ptr session;
	int requestid;

	XSMPlayFabPayload() : PlayFabPayloadBase(PlayFabPayloadType::XSM), user_id(0), requestid(-1) {}
	virtual ~XSMPlayFabPayload() {}
};

// Use some X-macro madness to keep these definitions together
// Define all new tasks types as part of this list!
// If it gets too big we can break it out into a separate file
#define DEF_TASKS	DEF_T(XSMTT_None, "None"), \
					DEF_T(XSMTT_CreateSession, "CreateSession"), \
					DEF_T(XSMTT_FindSession, "FindSession"), \
					DEF_T(XSMTT_JoinSession, "JoinSession"), \
					DEF_T(XSMTT_AdvertiseSession, "AdvertiseSession"), \
					DEF_T(XSMTT_MigrateHost, "MigrateHost"),


// Task types
/*enum 
{
	XSMTT_None,
	XSMTT_CreateSession,
	XSMTT_FindSession,	
	XSMTT_JoinSession,	
	XSMTT_AdvertiseSession,
	XSMTT_MigrateHost,
};*/

#define DEF_T(a, b) a
enum
{
	DEF_TASKS
};
#undef DEF_T

extern const char* g_XSMTaskNames[];

// Common task states
enum
{
	XSMTS_SetupPlayFabNetwork = -100,
	XSMTS_WaitForPlayFabNetworkSetup,
	XSMTS_WritePlayFabNetworkDetailsToSession,	

	XSMTS_GetPlayFabNetworkDetailsAndConnect,

	XSMTS_ConnectToPlayFabNetwork,
	XSMTS_WaitForConnectToPlayFabNetwork,
	XSMTS_WaitForAuthenticateLocalUser,
	XSMTS_WaitForConnectChatControl,
	XSMTS_WaitForCreateEndpoint,
	XSMTS_WriteUserPlayFabDetails,
	XSMTS_ConnectionToPlayFabNetworkCompleted,

	XSMTS_FailureCleanup = -2,
	XSMTS_Finished = -1,
	XSMTS_Unstarted = 0
};

class XSMtaskBase
{
public:		
	int taskType;
	int state;
	int64_t state_starttime;
	bool waiting;
	uint64_t user_id;		// not entirely sure this is going to cut it (not sure if we need user details even after a user has left)
	xbl_session_ptr session;
	Party::PartyNetworkDescriptor networkDescriptor;	
	Party::PartyNetwork* network;
	char invitationIdentifier[Party::c_maxInvitationIdentifierStringLength + 1];
	int requestid;

	virtual void Process();
	virtual void ProcessSessionChanged(xbl_session_ptr _updatedsession) {}
	virtual void ProcessSessionDeleted(xbl_session_ptr _sessiontodelete) {}

	virtual void ProcessPlayFabPartyXblChange(const Party::PartyXblStateChange* _change) {}
	virtual void ProcessPlayFabPartyChange(const Party::PartyStateChange* _change);

	virtual ~XSMtaskBase() {};

	void SetState(int _newstate);

	HRESULT WriteSessionAsync(XblContextHandle xblContext,
		XblMultiplayerSessionHandle multiplayerSession,
		XblMultiplayerSessionWriteMode writeMode,
		XAsyncCompletionRoutine* func,
		const char* optionalHandle = NULL);

	HRESULT SetSessionProperty(const char* _name, const char* _value);
	HRESULT SetSessionCurrentUserProperty(const char* _name, const char* _value);
	const char* GetSessionProperty(const char* _name);
	const char* GetSessionMemberProperty(const XblMultiplayerSessionMember* _member, const char* _name);
	const char* GetSessionUserProperty(uint64_t _user_id, const char* _name);
	HRESULT DeleteSessionProperty(const char* _name);
	
	HRESULT DeleteMatchTicketAsync(XblContextHandle xboxLiveContext,
		const char* serviceConfigurationId,
		const char* hopperName,
		const char* ticketId);

	bool IsXSMPlayFabPayloadForUs(XSMPlayFabPayload* _payload);	

protected:	
	virtual void SignalFailure();
	virtual void SignalDestroyed();
	//virtual void SignalMemberJoined(Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionMember^ _member, SecureDeviceAssociation^ _assoc);	
	virtual void SignalMemberJoined(uint64_t _user_id, const char* _entityID, bool _reportConnection);

protected:
	XSMtaskBase() { 
		taskType = XSMTT_None; 
		state = XSMTS_Unstarted; 
		state_starttime = -1; 
		waiting = false; 
		user_id = 0; 
		/*session = nullptr; */ 
		network = NULL; 
		requestid = -1; 
	}
};

extern const double MATCHMAKING_SESSION;
extern const double MATCHMAKING_INVITE;

#define TICKS_PER_MILLISECOND                               10000
#define TICKS_PER_SECOND                                    10000000i64

#include "CreateSession.h"
#include "FindSession.h"
#include "JoinSession.h"
#include "AdvertiseSession.h"
#include "MigrateHost.h"

class XSMsession
{
public:
	uint64_t owner_id;														// this is the local user whose Xbox Live Context was used for creating or retrieving this session	
	xbl_session_ptr session;	
	int id;
	//uint64_t changenumber;
	std::unordered_map<std::string, uint64_t> changemap;
	//SecureDeviceAssociationTemplate^ assoctemplate;
	const char* hopperName;											// if this session is advertised, this is the name of the hopper that is used for the ticket
	const char* matchAttributes;									// attributes used when creating/joining this session
	//Party::PartyNetwork* network;
	char playfabNetworkIdentifier[Party::c_networkIdentifierStringLength + 1];

	XSMsession()
	{ 
		owner_id = 0;
		id = -1;
		/*changenumber = 0;
		assoctemplate = nullptr;*/
		hopperName = NULL;
		matchAttributes = NULL;
		/*network = NULL; */
		playfabNetworkIdentifier[0] = '\0';
	}
	~XSMsession();
};

class SessionChangedRecord
{
public:
	uint64_t user_id;
	XblMultiplayerSessionChangeEventArgs args;
	//XblMultiplayerSessionReference sessionRef;
	//xbl_session_ptr newsession;
	//uint64_t changeNumber;

	SessionChangedRecord() { user_id = 0; ZeroMemory(&args, sizeof(XblMultiplayerSessionChangeEventArgs));/*ZeroMemory(&sessionRef, sizeof(sessionRef)); changeNumber = 0;*/ }
};

class XSM
{
	friend struct XSM_AutoMutex;
	friend struct XSM_Event_AutoMutex;

public:
	static void Init();
	static void Quit();

	static void Update();	

	static std::vector<XSMsession*>* GetSessions();
	static XSMsession* GetSession(uint32_t _id);
	static XSMsession* GetSession(xbl_session_ptr _session);
	static XSMsession* GetSession(uint64_t _user_id, XblMultiplayerSessionReference* _sessionref);
	static char* GetSessionName(xbl_session_ptr _session, char* _pBuffToUse);
	static int AddSession(xbl_session_ptr _session /*, SecureDeviceAssociationTemplate^ _assoctemplate*/, const char* _hopperName, const char* _matchAttributes);
	static void DeleteSession(uint32_t _id);
	static void DeleteSession(xbl_session_ptr _session);
	//static void ReplaceSession(Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ _srcsession, Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ _newsession);

	static uint64_t GetSessionUserIDFromGamerTag(const char* _gamerTag);
	static const char* GetSessionUserGamerTagFromID(uint64_t _user_id);
	static uint64_t GetSessionUserIDFromEntityID(const char* _entityID);
	static const char* GetSessionUserEntityIDFromID(uint64_t _user_id);	

	static bool IsUserHost(uint64_t _user_id, xbl_session_ptr _session);	
	static uint64_t GetHost(xbl_session_ptr _session);
	static XblDeviceToken GetUserDeviceToken(uint64_t _user_id, xbl_session_ptr _session);
	
	//static Windows::Foundation::Collections::IVector<XSMtaskBase^> ^GetTasks();
	static void AddTask(XSMtaskBase* _task);		

	static void DeleteSessionGlobally(xbl_session_ptr _session);

	static void OnSessionChanged(
		uint64_t _user_id,
		xbl_session_ptr _updatedsession
		);
	static void OnSessionChanged(
		uint64_t _user_id,
		XblMultiplayerSessionChangeEventArgs args
	);
	static void OnPlayFabPartyXblChange(
		const Party::PartyXblStateChange* _change
	);
	static void OnPlayFabPartyChange(
		const Party::PartyStateChange* _change
	);

	static void LockMutex();
	static void UnlockMutex();

	static void LockEventMutex();
	static void UnlockEventMutex();

	static int GetNextRequestID();

	static XblFormattedSecureDeviceAddress* GetDeviceAddress() { return &deviceAddress; }

	static void SignalMemberJoined(uint64_t _user_id, const char* _entityID, XSMsession* _session, bool _reportConnection);
	static void SignalMemberLeft(uint64_t _user_id, XSMsession* _session);

	static XTaskQueueHandle GetTaskQueue() { return taskQueue; }
private:
	static int GetNextSessionID();				

	static void ProcessMemberListChanged(XSMsession* _session, xbl_session_ptr _updatedsession);
	static void ProcessQueuedEvents();

	static std::vector<XSMsession*> *cachedSessions;
	static std::vector<XSMtaskBase*> *tasks;

	static std::vector<SessionChangedRecord*> *sessionChangedRecords;

	static int currSessionID;	
	static int currRequestID;

	static std::mutex mutex;
	static std::mutex eventMutex;

	static XTaskQueueHandle taskQueue;

	static XblFormattedSecureDeviceAddress deviceAddress;
	static XTaskQueueRegistrationToken inviteRegistration;
};

struct XSM_AutoMutex
{
	XSM_AutoMutex()
	{
		XSM::mutex.lock();
	}

	~XSM_AutoMutex()
	{
		XSM::mutex.unlock();
	}
};

#define XSM_LOCK_MUTEX XSM_AutoMutex __xsm_automutex;			// WARNING: If this is used on a thread that isn't the main thread you CANNOT have a DS_LOCK_MUTEX inside it or you risk deadlock

struct XSM_Event_AutoMutex
{
	XSM_Event_AutoMutex()
	{
		XSM::eventMutex.lock();
	}

	~XSM_Event_AutoMutex()
	{
		XSM::eventMutex.unlock();
	}
};

#define XSM_LOCK_EVENT_MUTEX XSM_Event_AutoMutex __xsm_event_automutex;

extern char* g_XboxSCID;		// we need this in a bunch of places

char* XuidToString(uint64_t _xuid);

#if defined(XSM_VERBOSE_TRACE)
#define XSM_VERBOSE_OUTPUT( msg, ...)	DebugConsoleOutput( msg, __VA_ARGS__ )
#else 
#define XSM_VERBOSE_OUTPUT( msg, ...)	
#endif

#endif // SESSIONMANAGEMENT_H