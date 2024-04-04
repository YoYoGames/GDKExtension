//
// Copyright (C) 2020 Opera Norway AS. All rights reserved.
//
// This file is an original work developed by Opera.
//

#include "GDKX.h"
#include "SessionManagement.h"
#include "SecureConnectionManager.h"
#include <ppltasks.h>
#include "DirectXHelper.h"
#include <json-c/json.h>
#include <objbase.h>
#include <XGameInvite.h>

#if !defined(WIN_UAP) && !defined(NO_SECURE_CONNECTION) && YY_CHAT
#include "Multiplayer/GameChat2IntegrationLayer.h"
#endif

#define QUEUE_CHANGED_EVENTS

#define PLAYFAB_NETWORK_DETAILS_TIMEOUT (30 * 1000 * 1000)			// 30 seconds


const double MATCHMAKING_SESSION = 3002;
const double MATCHMAKING_INVITE = 4001;

char* g_xboxLaunchURI = NULL;
char* g_xboxLaunchHost = NULL;
int64 g_xboxLaunchUser = 0;
bool g_shouldSendEventForInvite = false;

using namespace Windows::Foundation;
using namespace Concurrency;
using namespace Party;

extern bool g_XboxOneGameChatEnabled;

#define DEF_T(a, b) b
const char* g_XSMTaskNames[] = 
{
	DEF_TASKS
};
#undef DEF_T

HRESULT SetSessionProperty(xbl_session_ptr _session, const char* _name, const char* _value)
{
	if (_session == NULL)
		return -1;

	if (_name == NULL)
		return -1;

	if (_value == NULL)
		return -1;

	std::string jsonvalue = "\"";
	jsonvalue += _value;
	jsonvalue += "\"";

	HRESULT res;
	res = XblMultiplayerSessionSetCustomPropertyJson(_session->session_handle, _name, jsonvalue.c_str());
	return res;
}

HRESULT SetSessionCurrentUserProperty(xbl_session_ptr _session, const char* _name, const char* _value)
{
	if (_session == NULL)
		return -1;

	if (_name == NULL)
		return -1;

	if (_value == NULL)
		return -1;	

	std::string jsonvalue = "\"";
	jsonvalue += _value;
	jsonvalue += "\"";

	HRESULT res;
	res = XblMultiplayerSessionCurrentUserSetCustomPropertyJson(_session->session_handle, _name, jsonvalue.c_str());
	return res;
}

const char* GetSessionProperty(xbl_session_ptr _session, const char* _name)
{
	if (_session == NULL)
		return NULL;

	if (_name == NULL)
		return NULL;

	const XblMultiplayerSessionProperties* props = XblMultiplayerSessionSessionProperties(_session->session_handle);
	if (props == NULL)
	{		
		return NULL;
	}

	const char* valueString = NULL;

	json_object* pObj = NULL;
	pObj = json_tokener_parse(props->SessionCustomPropertiesJson);
	if (pObj != NULL)
	{
		json_object* valobj = json_object_object_get(pObj, _name);
		if (valobj != NULL)
		{
			json_type type = json_object_get_type(valobj);
			if (type == json_type_string)
			{
				const char* sval = json_object_get_string(valobj);
				if (sval != NULL)
				{
					valueString = YYStrDup(sval);
					return valueString;
				}
			}
		}		
		json_object_put(pObj);
	}

	return NULL;
}

const char* GetSessionMemberProperty(xbl_session_ptr _session, const XblMultiplayerSessionMember* _member, const char* _name)
{
	if (_session == NULL)
		return NULL;

	if (_member == NULL)
		return NULL;

	if (_name == NULL)
		return NULL;

	// Run through member list and try to find the specified user
	const char* valueString = NULL;

	if (_member->CustomPropertiesJson != NULL)
	{
		json_object* pObj = NULL;
		pObj = json_tokener_parse(_member->CustomPropertiesJson);
		if (pObj != NULL)
		{
			json_object* valobj = json_object_object_get(pObj, _name);
			if (valobj != NULL)
			{
				json_type type = json_object_get_type(valobj);
				if (type == json_type_string)
				{
					const char* sval = json_object_get_string(valobj);
					if (sval != NULL)
					{
						valueString = YYStrDup(sval);
						return valueString;
					}
				}
			}
			json_object_put(pObj);
		}

#ifdef XSM_VERBOSE_TRACE
		DebugConsoleOutput("GetSessionMemberProperty(): Couldn't read property %s for user %lld\n", _name, _member->Xuid);
#endif
	}
	return NULL;
}

const char* GetSessionUserProperty(xbl_session_ptr _session, uint64 _user_id, const char* _name)
{
	if (_session == NULL)
		return NULL;

	if (_name == NULL)
		return NULL;

	// Run through member list and try to find the specified user
	const XblMultiplayerSessionMember* memberList = NULL;
	size_t memberCount = 0;
	HRESULT res = XblMultiplayerSessionMembers(_session->session_handle, &memberList, &memberCount);
	if (SUCCEEDED(res))
	{
		for (int i = 0; i < memberCount; i++)
		{
			const XblMultiplayerSessionMember* member = &(memberList[i]);
			if (member->Xuid == _user_id)
			{
				return GetSessionMemberProperty(_session, member, _name);				
			}
		}

		XSM_VERBOSE_OUTPUT("GetSessionUserProperty(): User %lld not found in session\n", _user_id);
		return NULL;
	}

	XSM_VERBOSE_OUTPUT("GetSessionUserProperty(): Couldn't get member list for session\n");

	return NULL;
}

HRESULT DeleteSessionProperty(xbl_session_ptr _session, const char* _name)
{
	if (_session == NULL)
		return -1;

	if (_name == NULL)
		return -1;

	HRESULT res;
	res = XblMultiplayerSessionDeleteCustomPropertyJson(_session->session_handle, _name);
	return res;
}


XSMsession::~XSMsession()
{
	if (hopperName)
		YYFree(hopperName);
	if (matchAttributes)
		YYFree(matchAttributes);
}

void XSMtaskBase::SignalFailure()
{
	XSMsession* sess = XSM::GetSession(session);

	int dsMapIndex = CreateDsMap(5, "id",(double)MATCHMAKING_SESSION,(void *)NULL,
	"status", 0.0, "session_left",	
	"sessionid", sess != nullptr ? sess->id : -1.0, NULL,
	"requestid", (double) requestid, NULL,
	"error", -1.0, NULL);				

	CreateAsyncEventWithDSMap(dsMapIndex,EVENT_OTHER_SOCIAL);

	XSM_VERBOSE_OUTPUT("%s failed: request id %d\n", g_XSMTaskNames[taskType], requestid);
	
	SetState(XSMTS_Finished);
}

void XSMtaskBase::SignalDestroyed()
{
	XSMsession* sess = XSM::GetSession(session);

	int dsMapIndex = CreateDsMap(5, "id",(double)MATCHMAKING_SESSION,(void *)NULL,
	"status", 0.0, "session_destroyed",	
	"sessionid", sess != nullptr ? sess->id : -1.0, NULL,
	"requestid", (double) requestid, NULL,
	"error", 0.0, NULL);				

	CreateAsyncEventWithDSMap(dsMapIndex,EVENT_OTHER_SOCIAL);

	XSM_VERBOSE_OUTPUT("%s session destroyed: request id %d, session id %d\n", g_XSMTaskNames[taskType], requestid, sess != nullptr ? sess->id : -1);
	
	SetState(XSMTS_Finished);
}


void XSMtaskBase::SignalMemberJoined(uint64 _user_id, const char* _entityID, bool _reportConnection)
{
	XSMsession* sess = XSM::GetSession(session);

	if ((sess != NULL) && (PlayFabPartyManager::ExistsInNetworkUsers(sess->playfabNetworkIdentifier, _user_id)))
	{
		return;	// already added
	}

	// Get IP and port
	int error = 0;
	
	if (_entityID == NULL)	
	{
		error = -1;
	}

	// We don't have a host port
	int hostPort = 0;

#ifndef DISABLE_GAME_CHAT
	// Add remote user to chat
	if (error != -1)
	{
		OnChatSessionJoined(_member->XboxUserId, _hostIP);
	}
#endif	

	// Fire off a session_player_joined event
	int dsMapIndex = CreateDsMap(5, "id",(double)MATCHMAKING_SESSION,(void *)NULL,
	"status", 0.0, "session_player_joined",
	"sessionid", (double) ((sess != nullptr) ? sess->id : -1.0), NULL,		
	"requestid", (double) requestid, NULL,
	"error", (double)error, NULL);				

	// Add user id
	DsMapAddInt64(dsMapIndex, "userid", _user_id);

	CreateAsyncEventWithDSMap(dsMapIndex,EVENT_OTHER_SOCIAL);

	PlayFabPartyManager::AddRemoteChatUser(_user_id);

	if (_reportConnection)
	{
		// Now fire off a connection_info event
		dsMapIndex = CreateDsMap(7, "id", (double)MATCHMAKING_SESSION, (void*)NULL,
			"status", 0.0, "connection_info",
			"sessionid", (double)((sess != nullptr) ? sess->id : -1.0), NULL,
			"address", 0.0, _entityID != NULL ? _entityID : "",
			"port", (double)hostPort, NULL,
			"requestid", (double)requestid, NULL,
			"error", (double)error, NULL);

		// Add user id
		DsMapAddInt64(dsMapIndex, "userid", _user_id);

		CreateAsyncEventWithDSMap(dsMapIndex, EVENT_OTHER_SOCIAL);

		if ((sess != NULL) && (_entityID != NULL))
		{
			PlayFabPartyManager::AddToNetworkUsers(sess->playfabNetworkIdentifier, _entityID, _user_id);
		}
	}

#ifdef XSM_VERBOSE_TRACE
	char stringbuff[256];	
	DebugConsoleOutput("%s member joined: session id %s, request id %d, user id %I64u, IP %s, port %d\n", g_XSMTaskNames[taskType], XSM::GetSessionName(session, stringbuff), requestid, _user_id, _entityID != NULL ? _entityID : "", hostPort);
#endif	
}

int XSM::currSessionID = 0;
int XSM::currRequestID = 0;
std::vector<XSMsession*> *XSM::cachedSessions = NULL;
std::vector<XSMtaskBase*> *XSM::tasks = NULL;
std::vector<SessionChangedRecord*> *XSM::sessionChangedRecords = NULL;
std::mutex XSM::mutex;
std::mutex XSM::eventMutex;
XTaskQueueHandle XSM::taskQueue = NULL;
XblFormattedSecureDeviceAddress XSM::deviceAddress = { 0 };
XTaskQueueRegistrationToken XSM::inviteRegistration = { 0 };

void XSM::LockMutex()
{
	mutex.lock();
}

void XSM::UnlockMutex()
{
	mutex.unlock();
}

void XSM::LockEventMutex()
{
	eventMutex.lock();
}

void XSM::UnlockEventMutex()
{
	eventMutex.unlock();
}

int XSM::GetNextSessionID()
{
	int id = currSessionID;
	currSessionID++;

	if (currSessionID < 0)
	{		
		currSessionID = 0;
	}

	return id;
}

int XSM::GetNextRequestID()
{
	int id = currRequestID;
	currRequestID++;

	if (currRequestID < 0)
	{		
		currRequestID = 0;
	}

	return id;
}

void XSM::Init()
{
	cachedSessions = new std::vector<XSMsession*>();	
	tasks = new std::vector<XSMtaskBase*>();
	sessionChangedRecords = new std::vector<SessionChangedRecord*>();

	// Create a task queue that will process in the background on system threads and fire callbacks on a thread we choose in a serialized order
	DX::ThrowIfFailed(
		XTaskQueueCreate(XTaskQueueDispatchMode::ThreadPool, XTaskQueueDispatchMode::Manual, &taskQueue)
	);

	// Generate secure device address
	// It doesn't matter what it is, as long as it's unique to this device for the lifetime of any multiplayer sessions
	char address[64];
	GUID addressGUID;
	HRESULT res = CoCreateGuid(&addressGUID);
	extern void GUIDtoString(GUID * _guid, char* _outstring);
	GUIDtoString(&addressGUID, address);
	res = XblFormatSecureDeviceAddress(address, &deviceAddress);

#ifdef XSM_VERBOSE_TRACE
	HCSettingsSetTraceLevel(HCTraceLevel::Verbose); // See HCTraceLevel enum for various levels
	HCTraceSetTraceToDebugger(true);
#endif

	res = XGameInviteRegisterForEvent(
		XSM::GetTaskQueue(),
		NULL,
		[](void* context, const char* inviteUri)
		{
			g_xboxLaunchURI = (char*)YYRealloc(g_xboxLaunchURI, MAX_COMMAND_LINE);
			g_xboxLaunchHost = (char*)YYRealloc(g_xboxLaunchHost, MAX_COMMAND_LINE);

			// Crack URI to extract parts
			std::string inviteString(inviteUri);

			// First get session handle (this is common to both types of 'invite'
			size_t handlestart = inviteString.find("handle=") + 7;
			size_t handleend = inviteString.find("&", handlestart);

			if (handleend == std::string::npos)		// this happens if the handle is at the end of the string
			{
				handleend = inviteString.length() + 1;
			}

			std::string handlestring = inviteString.substr(handlestart, handleend - handlestart);
			strcpy(g_xboxLaunchURI, handlestring.c_str());

			// First figure out what we're dealing with
			if (inviteString.find("inviteHandleAccept") != std::string::npos)
			{
				sprintf(g_xboxLaunchHost, "inviteHandleAccept");		// this appears to be what the XDK puts in this field

				// Now figure out the invited user
				size_t userstart = inviteString.find("invitedXuid=") + 12;
				size_t userend = inviteString.find("&", userstart);

				if (userend == std::string::npos)		// this happens if the user is at the end of the string
				{
					userend = inviteString.length() + 1;
				}

				std::string userstring = inviteString.substr(userstart, userend - userstart);				
				g_xboxLaunchUser = std::stoull(userstring.c_str());
			}
			else
			{
				sprintf(g_xboxLaunchHost, "activityHandleJoin");		// this appears to be what the XDK puts in this field

				// Now figure out the invited user (technically the 'joiner' in this case)
				size_t userstart = inviteString.find("joinerXuid=") + 11;
				size_t userend = inviteString.find("&", userstart);

				if (userend == std::string::npos)		// this happens if the user is at the end of the string
				{
					userend = inviteString.length() + 1;
				}

				std::string userstring = inviteString.substr(userstart, userend - userstart);
				g_xboxLaunchUser = std::stoull(userstring.c_str());
			}

			if (g_shouldSendEventForInvite)
			{
				if (g_xboxLaunchURI && g_xboxLaunchHost)
				{
					int dsMapIndex = CreateDsMap(3,
						"id", MATCHMAKING_INVITE, (void*)NULL,
						"invitation_host", 0.0, g_xboxLaunchHost,
						"invitation_id", 0.0, g_xboxLaunchURI
					);

					// Add user id
					DsMapAddInt64(dsMapIndex, "invited_user", g_xboxLaunchUser);

					CreateAsyncEventWithDSMap(dsMapIndex, EVENT_OTHER_SOCIAL);
				}
			}
		},
		&inviteRegistration
	);

	if (FAILED(res))
	{
		DebugConsoleOutput("XSM::Init() - XGameInviteRegisterForEvent failed with 0x%08x\n", res);
	}
}

void XSM::Quit()
{
	delete cachedSessions;
	delete tasks;
	delete sessionChangedRecords;

	cachedSessions = NULL;
	tasks = NULL;
	sessionChangedRecords = NULL;

	XGameInviteUnregisterForEvent(inviteRegistration, false);

	XTaskQueueCloseHandle(taskQueue);
	taskQueue = NULL;
}

void XSM::Update()
{	
#ifdef QUEUE_CHANGED_EVENTS
	ProcessQueuedEvents();
#endif

	XSM_LOCK_MUTEX

	// Since tasks can complete asynchronously it's safest to remove them here
	for(int i = 0; i < tasks->size();)
	{
		XSMtaskBase* xsmtask = tasks->at(i);
		if (xsmtask->state == XSMTS_Finished)
		{
			delete xsmtask;
			tasks->erase(tasks->begin() + i);
		}
		else
		{
			if (xsmtask->waiting != true)
			{
				xsmtask->Process();
			}

			i++;
		}
	}

	// Handle callbacks in the main thread to ensure thread safety
	while (XTaskQueueDispatch(taskQueue, XTaskQueuePort::Completion, 0))
	{
	}
}

void XSM::ProcessQueuedEvents()
{
	// Copy event list so we don't need to hold a lock while processing it
	//Windows::Foundation::Collections::IVector<SessionChangedRecord^> ^recList = ref new Platform::Collections::Vector<SessionChangedRecord^>();
	std::vector<SessionChangedRecord*>* recList = new std::vector<SessionChangedRecord*>();

	LockEventMutex();

	size_t numrecords = sessionChangedRecords->size();
	for (size_t i = 0; i < numrecords; i++)
	{
		recList->push_back(sessionChangedRecords->at(i));
	}

	// Scrub the original list
	sessionChangedRecords->clear();

	UnlockEventMutex();

	// Now process
	for (int i = 0; i < numrecords; i++)
	{
		SessionChangedRecord* rec = recList->at(0);		// always get first record as we'll erase it when we're finished with it
		XSMsession* xsmsession = GetSession(rec->user_id, &(rec->args.SessionReference));

		XUMuser* xumuser = XUM::GetUserFromId(rec->user_id);
		XblContextHandle xbl_context = NULL;
		if (xumuser)
		{
			xbl_context = xumuser->GetXboxLiveContext();
		}

		if ((xsmsession != NULL) && (xbl_context != NULL))
		{
			// Sigh, looks like we can't guarantee that the branch GUID is NULL-terminated
			char branchID[XBL_GUID_LENGTH + 1];
			memcpy(branchID, rec->args.Branch, sizeof(char) * XBL_GUID_LENGTH);
			branchID[XBL_GUID_LENGTH] = '\0';

			auto branchval = xsmsession->changemap.find(branchID);
			if(branchval == xsmsession->changemap.end() || branchval->second < rec->args.ChangeNumber)
			{
				// We either haven't seen this branch before, or its change record is newer than what we've processed up to this point
				// It may be possible to get these records out of order but it shouldn't matter if we don't process intermediate versions
				// as long as we process the latest

				// Test
				const char* keystring = YYStrDup(branchID);
				xsmsession->changemap.erase(keystring);
				xsmsession->changemap.insert(std::make_pair(keystring, rec->args.ChangeNumber));

				// Need to get the latest version of the session from the backend now so we can compare it with what we have
				XAsyncBlock* asyncBlock = new XAsyncBlock();
				asyncBlock->queue = XSM::GetTaskQueue();
				asyncBlock->context = rec;
				asyncBlock->callback = [](XAsyncBlock* asyncBlock)
				{
					SessionChangedRecord* rec = (SessionChangedRecord*)(asyncBlock->context);

					XblMultiplayerSessionHandle sessionHandle;					
					HRESULT res = XblMultiplayerGetSessionResult(asyncBlock, &sessionHandle);
					if (SUCCEEDED(res))
					{
						xbl_session_ptr newsession = std::make_shared<xbl_session>(sessionHandle);

						XSMsession* xsmsession = GetSession(rec->user_id, &(rec->args.SessionReference));
						if (xsmsession != NULL)
						{
							const XblMultiplayerSessionInfo* sessinfo = XblMultiplayerSessionGetInfo(newsession->session_handle);
							if (sessinfo != NULL)
							{
								char branchID[XBL_GUID_LENGTH + 1];
								//memcpy(branchID, rec->args.Branch, sizeof(char) * XBL_GUID_LENGTH);
								memcpy(branchID, sessinfo->Branch, sizeof(char) * XBL_GUID_LENGTH);
								branchID[XBL_GUID_LENGTH] = '\0';
								// uint64* branchval = xsmsession->changemap.FindCheckKey(branchID);

								//if (memcmp(sessinfo->Branch, rec->args.Branch, sizeof(rec->args.Branch)) == 0)
								//if ((branchval == NULL) || (sessinfo->ChangeNumber > (*branchval)))
								if (1)
								{
//									if ((rec->args.ChangeNumber > sessinfo->ChangeNumber))
//									{
//#ifdef XSM_VERBOSE_TRACE
//										DebugConsoleOutput("ProcessQueuedEvents() session changed but this still isn't the latest version... %d > %d: session name %s\n", rec->args.ChangeNumber, sessinfo->ChangeNumber, rec->args.SessionReference.SessionName);
//#endif
//									}
//									else 
									{
										// Yay, things seem to check out, so update
										OnSessionChanged(rec->user_id, newsession);
																				
										//const char* keystring = YYStrDup(branchID);

										//xsmsession->changemap.DeleteCheckKey(keystring, [](const char** Key) { delete (*Key); }, [](uint64*) {});
										////xsmsession->changemap.Insert((char*)keystring, rec->args.ChangeNumber);
										//xsmsession->changemap.Insert((char*)keystring, sessinfo->ChangeNumber);
									}
								}
								else
								{
									XSM_VERBOSE_OUTPUT("ProcessQueuedEvents() branch doesn't match returned session: session name %s\n", rec->args.SessionReference.SessionName);
								}


							}
							else
							{
								XSM_VERBOSE_OUTPUT("ProcessQueuedEvents() couldn't get session info: session name %s\n", rec->args.SessionReference.SessionName);
							}
						}
					}
					else
					{
						XSM_VERBOSE_OUTPUT("ProcessQueuedEvents() get session failed 0x%08x: session name %s\n", res, rec->args.SessionReference.SessionName);
					}

					delete rec;
					delete asyncBlock;
				};

				HRESULT res = XblMultiplayerGetSessionAsync(xbl_context, &(rec->args.SessionReference), asyncBlock);
				if (FAILED(res))
				{
					DebugConsoleOutput("ProcessQueuedEvents() XblMultiplayerGetSessionAsync failed 0x%08x: session name %s\n", res, rec->args.SessionReference.SessionName);
					
					delete asyncBlock;
					delete rec;
				}
			}
			else
			{
				delete rec;
			}
		}
		else
		{
			delete rec;
		}
		
		recList->erase(recList->begin());
	}
}

std::vector<XSMsession*>* XSM::GetSessions()
{
	return cachedSessions;
}

XSMsession* XSM::GetSession(uint32 _id)
{
	XSM_LOCK_MUTEX

	size_t count = cachedSessions->size();
	for(size_t i = 0; i < count; i++)
	{
		if (cachedSessions->at(i)->id == _id)
		{
			return cachedSessions->at(i);
		}
	}

	return NULL;
}


bool AreSessionsRefsEqual(const XblMultiplayerSessionReference *ref, const XblMultiplayerSessionReference *ref2)
{
	//char string1[256], string2[256];
	//normalizeSessionName(ref->SessionName, string1);
	//normalizeSessionName(ref2->SessionName, string2);

	//return strcmp(string1, string2) == 0;
	return strcmp(ref->SessionName, ref2->SessionName) == 0;
}

bool AreSessionsRefsEqual(xbl_session_ptr ref, xbl_session_ptr ref2)
{
	return AreSessionsRefsEqual(XblMultiplayerSessionSessionReference(ref->session_handle), XblMultiplayerSessionSessionReference(ref2->session_handle));
}

bool AreSessionInitiatorsEqual(xbl_session_ptr ref, xbl_session_ptr ref2)
{
	const XblMultiplayerSessionConstants* ref_consts = XblMultiplayerSessionSessionConstants(ref->session_handle);
	const XblMultiplayerSessionConstants* ref2_consts = XblMultiplayerSessionSessionConstants(ref2->session_handle);

	if (ref_consts->InitiatorXuidsCount != ref2_consts->InitiatorXuidsCount)
		return false;
	for (int i = 0; i < ref_consts->InitiatorXuidsCount; i++)
	{
		if (ref_consts->InitiatorXuids[i] != ref2_consts->InitiatorXuids[i])
			return false;
	}

	return true;
}

bool AreSessionCurrentUsersEqual(xbl_session_ptr ref, xbl_session_ptr ref2)
{
	const XblMultiplayerSessionMember* currentuser = XblMultiplayerSessionCurrentUser(ref->session_handle);
	const XblMultiplayerSessionMember* currentuser2 = XblMultiplayerSessionCurrentUser(ref2->session_handle);

	if ((currentuser == NULL) && (currentuser2 == NULL))
		return true;
	if ((currentuser == NULL) || (currentuser2 == NULL))
		return false;
	if (currentuser->Xuid == currentuser2->Xuid)
		return true;

	return false;
}

bool AreSessionsEqual(xbl_session_ptr ref, xbl_session_ptr ref2, bool _comparecurrentusers = true, bool _allowNullRef2CurrentUser = false)
{
	bool refsequal = AreSessionsRefsEqual(ref, ref2);
	bool currentusersequal = true;
	if (_comparecurrentusers)
	{
		currentusersequal = AreSessionCurrentUsersEqual(ref, ref2);
		if (_allowNullRef2CurrentUser)
		{			
			const XblMultiplayerSessionMember* currentuser = XblMultiplayerSessionCurrentUser(ref2->session_handle);
			if ((currentuser == 0) || (currentuser->Xuid == 0))
				currentusersequal = true;	// pretend things are fine
		}
	}	
	/*
	if (_compareinitiators)
	{
		usersequal = AreSessionInitiatorsEqual(ref, ref2);
		if (_allowNullRef2Initiator)
		{
			const XblMultiplayerSessionConstants* ref2_consts = XblMultiplayerSessionSessionConstants(ref2->session_handle);
			if (ref2_consts->InitiatorXuidsCount == 0)
				usersequal = true;	// pretend things are fine
		}
	}
	*/

	return refsequal && currentusersequal;
}

XSMsession* XSM::GetSession(xbl_session_ptr _session)
{
	XSM_LOCK_MUTEX
	
	size_t count = cachedSessions->size();
	for(size_t i = 0; i < count; i++)
	{
		XSMsession* cachedSess = cachedSessions->at(i);		

		if (AreSessionsEqual(cachedSess->session, _session, true, true))
		{
			return cachedSessions->at(i);
		}
	}

	return nullptr;
}

XSMsession* XSM::GetSession(uint64 _user_id, XblMultiplayerSessionReference* _sessionref)
{
	XSM_LOCK_MUTEX	

	size_t count = cachedSessions->size();
	for(size_t i = 0; i < count; i++)
	{
		XSMsession* cachedSess = cachedSessions->at(i);		
		const XblMultiplayerSessionReference* cachedSessRef = XblMultiplayerSessionSessionReference(cachedSess->session->session_handle);

		if (strcmp(cachedSessRef->SessionName, _sessionref->SessionName) == 0)
		{
			const XblMultiplayerSessionMember* currentuser = XblMultiplayerSessionCurrentUser(cachedSess->session->session_handle);
			//if (currentuser->Xuid == _user_id)
			if ((currentuser == NULL) || (currentuser->Xuid == _user_id))
			{
				return cachedSessions->at(i);
			}
		}
	}

	return nullptr;
}

int XSM::AddSession(xbl_session_ptr _session/*, SecureDeviceAssociationTemplate^ _assoctemplate*/, const char* _hopperName, const char* _matchAttributes)
{	
	XSM_LOCK_MUTEX

	// Check for duplicate
	for(int i = 0; i < cachedSessions->size(); i++)
	{
		XSMsession* session = cachedSessions->at(i);

		if (session != nullptr)
		{			
			if (AreSessionsEqual(session->session, _session))
			{			
				return session->id;
			}
		}
	}

	// Didn't find the session already, so add a new one
	XSMsession *session = new XSMsession();
	session->session = _session;
	session->id = currSessionID++;	

	// Get the owner of this session
	const XblMultiplayerSessionMember* currentuser = XblMultiplayerSessionCurrentUser(_session->session_handle);
	if (currentuser == NULL)
	{
		session->owner_id = 0;
	}
	else
	{
		session->owner_id = currentuser->Xuid;
	}

	/*const XblMultiplayerSessionConstants* consts = XblMultiplayerSessionSessionConstants(_session->session_handle);
	if (consts->InitiatorXuidsCount == 0)
	{
		DebugConsoleOutput("XSM::AddSession() error: session has no initiating user (this should never happen)\n");
		session->owner_id = 0;
	}
	else
	{
		session->owner_id = consts->InitiatorXuids[0];
	}*/

	// Store the secure device association template - we need this to differentiate between multiple connections made to the same machine with different templates
	//session->assoctemplate = _assoctemplate;

	// Store the hopper name, if we have one
	session->hopperName = YYStrDup(_hopperName);
	session->matchAttributes = YYStrDup(_matchAttributes);

	cachedSessions->push_back(session);

	return session->id;
}

void XSM::DeleteSession(uint32 _id)
{	
	XSM_LOCK_MUTEX

	size_t count = cachedSessions->size();
	for(size_t i = 0; i < count; i++)
	{
		if (cachedSessions->at(i)->id == _id)
		{
			delete cachedSessions->at(i);
			cachedSessions->erase(cachedSessions->begin() + i);
			break;
		}
	}
}

void XSM::DeleteSession(xbl_session_ptr _session)
{
	XSM_LOCK_MUTEX

	size_t count = cachedSessions->size();
	for(size_t i = 0; i < count; i++)
	{
		if (AreSessionsEqual(cachedSessions->at(i)->session, _session))
		{
			PlayFabPartyManager::RemoveBufferedPacketsForNetwork(cachedSessions->at(i)->playfabNetworkIdentifier);

			// Run through member list and remove any remaining members
			const XblMultiplayerSessionMember* memberList = NULL;
			size_t memberCount = 0;
			HRESULT res = XblMultiplayerSessionMembers(cachedSessions->at(i)->session->session_handle, &memberList, &memberCount);
			if (SUCCEEDED(res))
			{
				for (int i = 0; i < memberCount; i++)
				{
					const XblMultiplayerSessionMember* member = &(memberList[i]);
					PlayFabPartyManager::RemoveRemoteChatUser(member->Xuid);
					PlayFabPartyManager::RemoveFromNetworkUsers(cachedSessions->at(i)->playfabNetworkIdentifier, member->Xuid);
				}
			}			

			delete cachedSessions->at(i);
			cachedSessions->erase(cachedSessions->begin() + i);
			break;
		}
	}
}


uint64 XSM::GetSessionUserIDFromGamerTag(const char* _gamerTag)
{
	if (_gamerTag == NULL)
		return 0;

	if (cachedSessions == NULL)
		return 0;

	for (int j = 0; j < cachedSessions->size(); j++)
	{
		XSMsession* xsm_session = cachedSessions->at(j);
		if ((xsm_session != NULL) && (xsm_session->session != NULL))
		{
			const XblMultiplayerSessionMember* memberList = NULL;
			size_t memberCount = 0;
			HRESULT res = XblMultiplayerSessionMembers(xsm_session->session->session_handle, &memberList, &memberCount);
			if (SUCCEEDED(res))
			{
				for (int i = 0; i < memberCount; i++)
				{
					const XblMultiplayerSessionMember* member = &(memberList[i]);
					if (strcmp(member->Gamertag, _gamerTag) == 0)
					{
						return member->Xuid;
					}
				}
			}
		}
	}

	return 0;
}

const char* XSM::GetSessionUserGamerTagFromID(uint64 _user_id)
{
	if (cachedSessions == NULL)
		return NULL;

	for (int j = 0; j < cachedSessions->size(); j++)
	{
		XSMsession* xsm_session = cachedSessions->at(j);
		if ((xsm_session != NULL) && (xsm_session->session != NULL))
		{
			const XblMultiplayerSessionMember* memberList = NULL;
			size_t memberCount = 0;
			HRESULT res = XblMultiplayerSessionMembers(xsm_session->session->session_handle, &memberList, &memberCount);
			if (SUCCEEDED(res))
			{
				for (int i = 0; i < memberCount; i++)
				{
					const XblMultiplayerSessionMember* member = &(memberList[i]);
					if (member->Xuid == _user_id)
					{
						const char* gamerTag = YYStrDup(member->Gamertag);
						return gamerTag;
					}
				}
			}
		}
	}

	return NULL;
}

uint64 XSM::GetSessionUserIDFromEntityID(const char* _entityID)
{
	if (cachedSessions == NULL)
		return 0;

	for (int j = 0; j < cachedSessions->size(); j++)
	{
		XSMsession* xsm_session = cachedSessions->at(j);
		if ((xsm_session != NULL) && (xsm_session->session != NULL))
		{
			// First see if the user is active in the session
			const XblMultiplayerSessionMember* memberList = NULL;
			size_t memberCount = 0;
			HRESULT res = XblMultiplayerSessionMembers(xsm_session->session->session_handle, &memberList, &memberCount);
			if (SUCCEEDED(res))
			{
				for (int i = 0; i < memberCount; i++)
				{
					const XblMultiplayerSessionMember* member = &(memberList[i]);
										
					const char* entityID = GetSessionMemberProperty(xsm_session->session, member, "entityID");
					if (strcmp(entityID, _entityID) == 0)						
					{
						YYFree(entityID);
						return member->Xuid;
					}					
					YYFree(entityID);
				}
			}
		}
	}

	return 0;
}

const char* XSM::GetSessionUserEntityIDFromID(uint64 _user_id)
{
	if (cachedSessions == NULL)
		return NULL;

	for (int j = 0; j < cachedSessions->size(); j++)
	{
		XSMsession* xsm_session = cachedSessions->at(j);
		if ((xsm_session != NULL) && (xsm_session->session != NULL))
		{
			// First see if the user is active in the session
			const XblMultiplayerSessionMember* memberList = NULL;
			size_t memberCount = 0;
			HRESULT res = XblMultiplayerSessionMembers(xsm_session->session->session_handle, &memberList, &memberCount);
			if (SUCCEEDED(res))
			{
				for (int i = 0; i < memberCount; i++)
				{
					const XblMultiplayerSessionMember* member = &(memberList[i]);
					if (member->Xuid == _user_id)
					{
						// Okay, the user is in the session, so look for the entity ID
						const char* entityID = GetSessionMemberProperty(xsm_session->session, member, "entityID");
						if (entityID != NULL)
						{							
							return entityID;
						}
					}
				}
			}				
		}
	}

	return NULL;
}

bool XSM::IsUserHost(uint64 _user_id, xbl_session_ptr _session)
{
	if ((_user_id == 0) || (_session == NULL))
		return false;

	// First check to see if the session actually has a host
	const XblMultiplayerSessionProperties* props = XblMultiplayerSessionSessionProperties(_session->session_handle);
	if (props == NULL)
		return false;

	if (props->HostDeviceToken.Value[0] == '\0')
		return false;	

	const char* hostToken = props->HostDeviceToken.Value;

	// Now find the user in the member list and see if the device token matches
	const XblMultiplayerSessionMember* memberList = NULL;
	size_t memberCount = 0;
	HRESULT res = XblMultiplayerSessionMembers(_session->session_handle, &memberList, &memberCount);
	if (SUCCEEDED(res))
	{		
		for (int i = 0; i < memberCount; i++)
		{
			const XblMultiplayerSessionMember* member = &(memberList[i]);
			if (member->Xuid == _user_id)
			{
				if (_stricmp(member->DeviceToken.Value, hostToken) == 0)				
				{
					return true;
				}
			}
		}
	}

	return false;
}

uint64 XSM::GetHost(xbl_session_ptr _session)
{
	if (_session == NULL)
		return 0;

	// First check to see if the session actually has a host
	const XblMultiplayerSessionProperties* props = XblMultiplayerSessionSessionProperties(_session->session_handle);
	if (props == NULL)
		return 0;

	if (props->HostDeviceToken.Value[0] == '\0')
		return 0;

	const char* hostToken = props->HostDeviceToken.Value;

	// Now find the first user in the member list that matches the device token - that will be the host
	const XblMultiplayerSessionMember* memberList = NULL;
	size_t memberCount = 0;
	HRESULT res = XblMultiplayerSessionMembers(_session->session_handle, &memberList, &memberCount);
	if (SUCCEEDED(res))
	{
		for (int i = 0; i < memberCount; i++)
		{
			const XblMultiplayerSessionMember* member = &(memberList[i]);
			if (_stricmp(member->DeviceToken.Value, hostToken) == 0)
			{
				return member->Xuid;
			}			
		}
	}

	return 0;
}

XblDeviceToken XSM::GetUserDeviceToken(uint64 _user_id, xbl_session_ptr _session)
{
	XblDeviceToken emptytoken = { 0 };

	if ((_user_id == 0) || (_session == NULL))
		return emptytoken;

	const XblMultiplayerSessionProperties* props = XblMultiplayerSessionSessionProperties(_session->session_handle);
	if (props == NULL)
		return emptytoken;

	// Now find the user in the member list and return its device token
	const XblMultiplayerSessionMember* memberList = NULL;
	size_t memberCount = 0;
	HRESULT res = XblMultiplayerSessionMembers(_session->session_handle, &memberList, &memberCount);
	if (SUCCEEDED(res))
	{
		for (int i = 0; i < memberCount; i++)
		{
			const XblMultiplayerSessionMember* member = &(memberList[i]);
			if (member->Xuid == _user_id)
			{
				return member->DeviceToken;				
			}
		}
	}

	return emptytoken;
}

void XSM::AddTask(XSMtaskBase* _context)
{
	tasks->push_back(_context);
}

void XSM::DeleteSessionGlobally(xbl_session_ptr _session)
{
	// We'll let the calling function inform the user since it knows the context in which the session is being deleted
	// Loop through all contexts and call their delete handler
	XSM_LOCK_MUTEX
	
	size_t count = tasks->size();

	size_t i;
	for(i = 0; i < count; i++)
	{
		XSMtaskBase* xsmtask = tasks->at(i);

		if (xsmtask != NULL)
		{
			// First compare user which 'initiated' this event to avoid processing events multiple times if we have multiple event handlers hooked up to different users
			if (xsmtask->state != XSMTS_Finished)
			{
				// Check that this event applies to the session associated with this context
				if (AreSessionsEqual(_session, xsmtask->session, true, false))
				{
					xsmtask->ProcessSessionDeleted(_session);					
				}
			}
		}
	}

	// Then go through all our stored sessions and nuke it when we find it	
	DeleteSession(_session);
}

char* XSM::GetSessionName(xbl_session_ptr _session, char* _pBuffToUse)
{
	const XblMultiplayerSessionReference* ref = NULL;
	if (_session != NULL)
	{
		ref = XblMultiplayerSessionSessionReference(_session->session_handle);
	}

	if (ref != nullptr)
	{
		strcpy(_pBuffToUse, ref->SessionName);		
	}
	else
	{		
		strcpy(_pBuffToUse, "None");
	}

	return _pBuffToUse;
}


void XSM::SignalMemberJoined(uint64 _user_id, const char* _entityID, XSMsession* _session, bool _reportConnection)
{	
	if ((_session != NULL) && (PlayFabPartyManager::ExistsInNetworkUsers(_session->playfabNetworkIdentifier, _user_id)))
	{
		return;	// already added
	}

	// Get IP and port
	int error = 0;
	
	if (_entityID == NULL)	
	{
		error = -1;
	}

	// Host port is meaningless here
	int hostPort = 0;	

#ifndef DISABLE_GAME_CHAT
	// Add remote user to chat
	if (error != -1)
	{
		OnChatSessionJoined(_member->XboxUserId, _hostIP);
	}
#endif 

	// Fire off a session_player_joined event
	int dsMapIndex = CreateDsMap(4, "id",(double)MATCHMAKING_SESSION,(void *)NULL,
	"status", 0.0, "session_player_joined",
	"sessionid", (double) ((_session != NULL) ? _session->id : -1.0), NULL,
	"error", (double)error, NULL);				

	// Add user id
	DsMapAddInt64(dsMapIndex, "userid", _user_id);

	CreateAsyncEventWithDSMap(dsMapIndex,EVENT_OTHER_SOCIAL);

	PlayFabPartyManager::AddRemoteChatUser(_user_id);

	if (_reportConnection)
	{
		// Now fire off a connection_info event`
		dsMapIndex = CreateDsMap(6, "id", (double)MATCHMAKING_SESSION, (void*)NULL,
			"status", 0.0, "connection_info",
			"sessionid", (double)((_session != nullptr) ? _session->id : -1.0), NULL,
			"address", 0.0, _entityID != NULL ? _entityID : "",
			"port", (double)hostPort, NULL,
			"error", (double)error, NULL);

		// Add user id
		DsMapAddInt64(dsMapIndex, "userid", _user_id);

		CreateAsyncEventWithDSMap(dsMapIndex, EVENT_OTHER_SOCIAL);

		if ((_session != NULL) && (_entityID != NULL))
		{
			PlayFabPartyManager::AddToNetworkUsers(_session->playfabNetworkIdentifier, _entityID, _user_id);
		}
	}

#ifdef XSM_VERBOSE_TRACE
	char stringbuff[256];
	DebugConsoleOutput("Member joined session: session id %s, user id %I64u, IP %s, port %d\n", GetSessionName(_session->session, stringbuff), _user_id, _entityID != NULL ? _entityID : "", hostPort);
#endif	
}

void XSM::SignalMemberLeft(uint64 _user_id, XSMsession* _session)
{
	int error = 0;

	// Convert xbox live id to a uint64

	int dsMapIndex = CreateDsMap(4, "id",(double)MATCHMAKING_SESSION,(void *)NULL,
	"status", 0.0, "session_player_left",
	"sessionid", (double) ((_session != NULL) ? _session->id : -1.0), NULL,			
	"error", (double)error, NULL);				

	// Add user id
	DsMapAddInt64(dsMapIndex, "userid", _user_id);

	CreateAsyncEventWithDSMap(dsMapIndex,EVENT_OTHER_SOCIAL);

	PlayFabPartyManager::RemoveRemoteChatUser(_user_id);

	if (_session != NULL)
	{
		PlayFabPartyManager::RemoveBufferedPacketsForUser(_session->playfabNetworkIdentifier, _user_id);
		PlayFabPartyManager::RemoveFromNetworkUsers(_session->playfabNetworkIdentifier, _user_id);
	}
}

#include <Robuffer.h>
// #include "Files/Buffer/Buffer_Manager.h"
extern void F_NETWORK_Set_Config(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg);
extern void F_NETWORK_Create_Socket_Ext(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg);
extern void F_NETWORK_Send_UDP(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg);


// ------------------------------------------------------------

#if !defined(WIN_UAP) && !defined(NO_SECURE_CONNECTION) && YY_CHAT

extern bool g_ChatIntegrationShuttingDown;
extern bool g_chatIntegrationLayerInited;
extern bool g_XboxOneGameChatEnabled;

namespace XboxOneChat
{
	int g_xboxChatSocketId = -1;
	int g_voicePort = -1;

	void Shutdown()
	{
		if(g_XboxOneGameChatEnabled && g_chatIntegrationLayerInited && !g_ChatIntegrationShuttingDown)
			GameChat2IntegrationLayer::Get()->Shutdown();
	}

	void InitializeSocket()
	{
		SecureDeviceAssociationTemplate^ sdaTemplate = nullptr;
		try
		{
			sdaTemplate = SecureDeviceAssociationTemplate::GetTemplateByName(L"GameChatTraffic");
		}
		catch(Platform::Exception ^ex)
		{
			DebugConsoleOutput("Error: Could not retrieve GameChatTraffic Secure Device Association Template\n");
			return;
		}

		GameChat2IntegrationLayer::Get()->InitializeChatManager(false, false);

		if (XboxOneChat::g_xboxChatSocketId < 0)
		{
			eSocketType socktype = eSocketType_udp;
			RValue res;
			RValue params[2];

			params[0].kind = VALUE_REAL;
			params[0].val = socktype;

			g_voicePort = sdaTemplate->AcceptorSocketDescription->BoundPortRangeLower;

			params[1].kind = VALUE_REAL;
			params[1].val = g_voicePort;

			F_NETWORK_Create_Socket_Ext(res, NULL, NULL, 2, params);

			if (res.kind == VALUE_REAL && (res.val >= 0))
			{
				XboxOneChat::g_xboxChatSocketId = res.val;

				params[0].val = ENC_USE_RELIABLE_UDP_ON_SOCKET;
				params[1].val = XboxOneChat::g_xboxChatSocketId;

				F_NETWORK_Set_Config(res, NULL, NULL, 2, params);
			}
		}
	}

	void GetBufferBytes( __in Windows::Storage::Streams::IBuffer^ buffer, __out byte** ppOut )
	{
		if ( ppOut == nullptr || buffer == nullptr )
		{
			throw ref new Platform::InvalidArgumentException();
		}

		*ppOut = nullptr;

		Microsoft::WRL::ComPtr<IInspectable> srcBufferInspectable(reinterpret_cast<IInspectable*>( buffer ));
		Microsoft::WRL::ComPtr<Windows::Storage::Streams::IBufferByteAccess> srcBufferByteAccess;
		srcBufferInspectable.As(&srcBufferByteAccess);
		srcBufferByteAccess->Buffer(ppOut);
	}

	void ProcessIncomingPacket(unsigned char *buff, int size, char *_addr, int port)
	{
		if (size < 0)
			return;

		SecureDeviceAssociationTemplate^ sdaTemplate = SecureDeviceAssociationTemplate::GetTemplateByName(L"GameChatTraffic");
		SecureConnection ^conn = nullptr;
		
		SCM_LOCK_MUTEX

		Windows::Foundation::Collections::IVector<SecureConnection^> ^conns = SCM::GetConnections();

		char realaddr[1024];
		strcpy(realaddr, _addr + 1);
		*strrchr(realaddr, ']') = '\0';

		Platform::String ^addr = ConvertCharArrayToManagedString(realaddr);

		for each (SecureConnection^ member in conns)
		{
			if (!member->assocTemplate->Name->Equals(sdaTemplate->Name))
				continue;

			if ((member->association != nullptr) && (Windows::Networking::HostName::Compare(member->association->RemoteHostName->CanonicalName, addr) == 0))
			{
				conn = member;
				break;
			}
		}

		if (conn == nullptr)
		{
			DebugConsoleOutput("Could not identify connection for packet from %s... Options were:\n", _addr);
			for each (SecureConnection^ member in conns)
			{
				if(member->association != nullptr) DebugConsoleOutput("- '%S'\n", member->association->RemoteHostName->RawName->Data());
				else DebugConsoleOutput("- 'no association!'\n");
			}
			return;
		}

		Windows::Storage::Streams::IBuffer^ destBuffer = ref new Windows::Storage::Streams::Buffer( size );
		byte* destBufferBytes = nullptr;
		GetBufferBytes( destBuffer, &destBufferBytes );
		errno_t err = memcpy_s( destBufferBytes, destBuffer->Capacity, buff, size );
		if (err != 0)
		{
			DebugConsoleOutput("Could not copy voice packet data into IBuffer\n", _addr);
			return;
		}
		destBuffer->Length = size;

		GameChat2IntegrationLayer::Get()->OnIncomingChatMessage(destBuffer, conn->association->RemoteHostName->RawName);
	}

	void SendChatMessage(SecureConnection ^sc, Windows::Storage::Streams::IBuffer ^buffer, bool reliable)
	{
		if (sc->association == nullptr) return;
		
		unsigned char* destBufferBytes = nullptr;
		GetBufferBytes(buffer, &destBufferBytes);

		int buffer_index = CreateBuffer(buffer->Length, eBuffer_Format_Fast, 0);
		IBuffer* pBuff = GetIBuffer(buffer_index);
		unsigned char* pData = pBuff->GetBuffer();
		memcpy(pData, destBufferBytes, buffer->Length);

		char nameBuffer[1024];
		char portBuffer[1024];

		RValue res;
		RValue params[5];

		params[0].kind = VALUE_REAL;
		params[0].val = g_xboxChatSocketId;

		ConvertFromWideCharToUTF8((wchar_t*)(sc->association->RemoteHostName->RawName->Data()), nameBuffer);
		YYCreateString( &params[1], nameBuffer );

		ConvertFromWideCharToUTF8((wchar_t*)(sc->association->RemotePort->Data()), portBuffer);
		int portNumber = atoi(portBuffer);
		params[2].kind = VALUE_REAL;
		params[2].val = portNumber;

		params[3].kind = VALUE_REAL;
		params[3].val = buffer_index;

		params[4].kind = VALUE_REAL;
		params[4].val = buffer->Length;

		F_NETWORK_Send_UDP(res, NULL, NULL, 5, params);

		FreeIBuffer(buffer_index);
	}
}
#endif



void XSM::ProcessMemberListChanged(XSMsession* _session, xbl_session_ptr _updatedsession)
{
#ifdef XSM_VERBOSE_TRACE
	char stringbuff[256];
	DebugConsoleOutput("ProcessMemberListChanged : session id %d, session name %s\n", _session->id, GetSessionName(_session->session, stringbuff));
#endif

	HRESULT res;

	const XblMultiplayerSessionMember* oldMemberList = NULL;
	const XblMultiplayerSessionMember* newMemberList = NULL;
	size_t oldMemberCount = 0;
	size_t newMemberCount = 0;

	res = XblMultiplayerSessionMembers(_session->session->session_handle, &oldMemberList, &oldMemberCount);
	if (FAILED(res))
	{
		XSM_VERBOSE_OUTPUT("XSM::ProcessMemberListChanged: XblMultiplayerSessionMembers() couldn't get old member list: error 0x%08x\n", res);
		return;
	}

	res = XblMultiplayerSessionMembers(_updatedsession->session_handle, &newMemberList, &newMemberCount);
	if (FAILED(res))
	{
		XSM_VERBOSE_OUTPUT("XSM::ProcessMemberListChanged: XblMultiplayerSessionMembers() couldn't get new member list: error 0x%08x\n", res);
		return;
	}

	// Logic:
	// Class a user as new if they are a member in the updated session *and* there is a custom session property for their Xuid to Entity ID mapping that doesn't exist in the previous version of the session
	// (or where the value of the Xuid to Entity ID is different (this should handle rejoin scenarios)
	// Class a user as removed if they existed in the old session and had a Xuid to Entity ID custom property, but they don't exist in the new session
	// NOTE: Do we still want to maintain a logical star topology as we use on Xbox and PS4? This means that only the host is informed of all players
	// while 'clients' are only informed of a connection to the host. This seems a bit restricting and it's actually more complicated for us to handle
	// then a pure mesh connection but it would be consistent with the other platforms.

	std::vector<const XblMultiplayerSessionMember*> membersadded;
	std::vector<const XblMultiplayerSessionMember*> membersremoved;

	// First work out which members have been removed	
	for (int i = 0; i < oldMemberCount; i++)
	{
		const XblMultiplayerSessionMember* member = &(oldMemberList[i]);
		const char* entityID = GetSessionMemberProperty(_session->session, member, "entityID");
		if (entityID == NULL)
		{
			// This user doesn't have a valid entity string in the session document, so it hasn't been fully set up yet			
			continue;
		}
		YYFree(entityID);		// at the moment we don't care what the value is, just that it exists

		bool found = false;
		for (int j = 0; j < newMemberCount; j++)
		{
			const XblMultiplayerSessionMember* newmember = &(newMemberList[j]);
			if (newmember->Xuid == member->Xuid)
			{
				found = true;
				break;
			}
		}

		if (!found)
		{
			membersremoved.push_back(member);
		}
	}

	// Now see which have been added
	for (int i = 0; i < newMemberCount; i++)
	{
		const XblMultiplayerSessionMember* member = &(newMemberList[i]);
		const char* entityID = GetSessionMemberProperty(_updatedsession, member, "entityID");
		if (entityID == NULL)
		{
			// This user doesn't have a valid entity string in the session document, so it hasn't been fully set up yet			
			continue;
		}

		bool found = false;
		for (int j = 0; j < oldMemberCount; j++)
		{
			const XblMultiplayerSessionMember* oldmember = &(oldMemberList[j]);
			if (oldmember->Xuid == member->Xuid)
			{
				const char* oldEntityID = GetSessionMemberProperty(_session->session, oldmember, "entityID");
				if (oldEntityID != NULL)
				{
					if (strcmp(entityID, oldEntityID) == 0)
					{
						found = true;
					}

					YYFree(oldEntityID);
				}

				break;		// we shouldn't find any more users in the old session with the same XUID
			}
		}

		if (!found)
		{
			membersadded.push_back(member);
		}

		YYFree(entityID);
	}


	// If we're forcing star topology:
	// If the current user is the host, report all connections and disconnections
	// If the current user is not the host, report only connections and disconnections from the active host
	// - If the current user is not the host, and the host has been removed, start up a host migration task
	// - (don't do this if the host wasn't set in the old state, as we don't want to spin up loads of migration tasks)

	// If we're not forcing star topology:
	// Report all connections and disconnections, regardless of whether the current user is the host or not
	// If the host has been removed, start up a host migration task

	// Determine if this machine is the host
	// Get the host device token and compare it to the current user's
	bool IsHost = false;

#ifndef ENFORCE_STAR_NETWORK_TOPOLOGY

#else

	const XblMultiplayerSessionProperties* props = XblMultiplayerSessionSessionProperties(_updatedsession->session_handle);
	if (props == NULL)
	{
		XSM_VERBOSE_OUTPUT("XSM::ProcessMemberListChanged: XblMultiplayerSessionSessionProperties() returned NULL\n");
		return;
	}

	const XblMultiplayerSessionMember* currentuser = XblMultiplayerSessionCurrentUser(_updatedsession->session_handle);
	if (currentuser == NULL)
	{
		XSM_VERBOSE_OUTPUT("XSM::ProcessMemberListChanged: XblMultiplayerSessionCurrentUser() returned NULL\n");
		return;
	}

	if (_stricmp(props->HostDeviceToken.Value, currentuser->DeviceToken.Value) == 0)
	{
		IsHost = true;
	}

	if (IsHost)
	{
		for (int i = 0; i < membersremoved.size(); i++)
		{
			const XblMultiplayerSessionMember* member = membersremoved.at(i);
			SignalMemberLeft(member->Xuid, _session);
		}

		for (int i = 0; i < membersadded.size(); i++)
		{
			const XblMultiplayerSessionMember* member = membersadded.at(i);
			if (_stricmp(props->HostDeviceToken.Value, member->DeviceToken.Value) == 0)
			{
				// This member is on the same machine as the host so ignore it (same as the XDK version)
				continue;
			}

			const char* entityID = GetSessionMemberProperty(_updatedsession, member, "entityID");
			if (entityID == NULL)
			{
				// This user doesn't have a valid entity string in the session document, so it hasn't been fully set up yet				
				continue;
			}

			SignalMemberJoined(member->Xuid, entityID, _session, true);

			YYFree(entityID);
		}
	}
	else
	{

#ifdef REPORT_SESSION_JOINED_FOR_ALL_MEMBERS

		// First handle players leaving

		const XblMultiplayerSessionProperties* oldprops = XblMultiplayerSessionSessionProperties(_session->session->session_handle);
		if (oldprops == NULL)
		{
			XSM_VERBOSE_OUTPUT("XSM::ProcessMemberListChanged: XblMultiplayerSessionSessionProperties() returned NULL\n");
			return;
		}

		// Figure out who the host was, and see if they've left		
		const XblMultiplayerSessionMember* origHost = NULL;
		if (oldprops->HostDeviceToken.Value[0] == '\0')
		{
			// Okay, no host was set previously, so we don't need to do anything here
			XSM_VERBOSE_OUTPUT("No host found in original session\n");
		}
		else
		{
			// Get the original host device token
			const char* hostToken = oldprops->HostDeviceToken.Value;

			// check to see if the original host has been removed
			// The original host will be the first one matching the host device token in the old session			
			for (int i = 0; i < oldMemberCount; i++)
			{
				const XblMultiplayerSessionMember* member = &(oldMemberList[i]);
				if (_stricmp(member->DeviceToken.Value, hostToken) == 0)
				{
					origHost = member;
					break;
				}
			}
		}
		
		for (int i = 0; i < membersremoved.size(); i++)
		{
			const XblMultiplayerSessionMember* member = membersremoved.at(i);
			
			// Signal disconnect
			SignalMemberLeft(member->Xuid, _session);

			if ((origHost != NULL) || (member->Xuid == origHost->Xuid))
			{
				// As the host has left, crank up the host migration process
				XSMtaskMigrateHost* migratetask = new XSMtaskMigrateHost();
				migratetask->session = _updatedsession;
				migratetask->user_id = _session->owner_id;
				migratetask->hopperName = YYStrDup(_session->hopperName);
				//migratetask->sdaTemplateName = _session->assoctemplate->Name;
				migratetask->matchAttributes = YYStrDup(_session->matchAttributes);
				migratetask->requestid = XSM::GetNextRequestID();
				migratetask->SetState(XSMTS_MigrateHost_SetHost);

				XSM::AddTask(migratetask);
			}			
		}

		// Now handle players joining		
		// Get new host (if there is one)
		const XblMultiplayerSessionMember* host = NULL;
		if (props->HostDeviceToken.Value[0] == '\0')
		{
			XSM_VERBOSE_OUTPUT("No host found in updated session\n");
		}
		else
		{
			// Get new host token
			const char* hostToken = props->HostDeviceToken.Value;

			// Iterate through the added member list and find a session member with this token (there may be multiple members with the same token if they're on the same machine - currently just pick the first one)			
			for (int i = 0; i < membersadded.size(); i++)
			{
				const XblMultiplayerSessionMember* member = membersadded.at(i);
				if (_stricmp(member->DeviceToken.Value, hostToken) == 0)
				{
					host = member;
					break;
				}
			}
		}

		for (int i = 0; i < membersadded.size(); i++)
		{
			const XblMultiplayerSessionMember* member = membersadded.at(i);
			if (_session->owner_id != member->Xuid)		// Filter out the current user (I can't see how they could be in this list anyway)
			{
				// Get entity ID from the member, and signal that they have joined		
				const char* entityID = GetSessionMemberProperty(_updatedsession, member, "entityID");
				if (entityID == NULL)
				{
					// This user doesn't have a valid entity string in the session document, so it hasn't been fully set up yet									
					continue;
				}

				if ((host != NULL) && (member->Xuid == host->Xuid))
					SignalMemberJoined(member->Xuid, entityID, _session, true);		// signal connection to host
				else
					SignalMemberJoined(member->Xuid, entityID, _session, false);		// don't signal connection info for any user that isn't the host

				YYFree(entityID);
			}
		}
#else

		const XblMultiplayerSessionProperties* oldprops = XblMultiplayerSessionSessionProperties(_session->session->session_handle);
		if (oldprops == NULL)
		{
			XSM_VERBOSE_OUTPUT("XSM::ProcessMemberListChanged: XblMultiplayerSessionSessionProperties() returned NULL\n");
			return;
		}

		// Figure out who the host was, and see if they've left		
		const XblMultiplayerSessionMember* origHost = NULL;
		if (oldprops->HostDeviceToken.Value[0] == '\0')
		{
			// Okay, no host was set previously, so we don't need to do anything here
			XSM_VERBOSE_OUTPUT("No host found in original session\n");
		}
		else
		{
			// Get the original host device token
			const char* hostToken = oldprops->HostDeviceToken.Value;

			// check to see if the original host has been removed
			// The original host will be the first one matching the host device token in the old session			
			for (int i = 0; i < oldMemberCount; i++)
			{
				const XblMultiplayerSessionMember* member = &(oldMemberList[i]);
				if (stricmp(member->DeviceToken.Value, hostToken) == 0)
				{
					origHost = member;
					break;
				}
			}

			if (origHost != NULL)
			{
				// See if the host is in the membersremoved list
				for (int i = 0; i < membersremoved.size(); i++)
				{
					const XblMultiplayerSessionMember* member = membersremoved.at(i);
					if (member->Xuid == origHost->Xuid)
					{						
						// Signal disconnect
						SignalMemberLeft(member->Xuid, _session);

						// Crank up the host migration process
						XSMtaskMigrateHost* migratetask = new XSMtaskMigrateHost();
						migratetask->session = _updatedsession;
						migratetask->user_id = _session->owner_id;						
						migratetask->hopperName = YYStrDup(_session->hopperName);
						//migratetask->sdaTemplateName = _session->assoctemplate->Name;
						migratetask->matchAttributes = YYStrDup(_session->matchAttributes);
						migratetask->requestid = XSM::GetNextRequestID();
						migratetask->SetState(XSMTS_MigrateHost_SetHost);

						XSM::AddTask(migratetask);

						break;
					}
				}
			}
		}

		// Now handle if new host machine has been added		

		// Wait for a secure device association to be set up with the host
		// At the moment we're enforcing a star topology so all clients can only communicate with the host
		// Need a timeout here in case there's some sort of problem
		// What to do if there is a timeout failure?
		// Nothing, at the moment		

		// First check that there actually is a host
		const XblMultiplayerSessionMember* host = NULL;
		if (props->HostDeviceToken.Value[0] == '\0')		
		{
			XSM_VERBOSE_OUTPUT("No host found in updated session\n");
		}
		else
		{
			// Get new host token
			const char* hostToken = props->HostDeviceToken.Value;						

			// Iterate through the added member list and find a session member with this token (there may be multiple members with the same token if they're on the same machine - currently just pick the first one)			
			for (int i = 0; i < membersadded.size(); i++)
			{
				const XblMultiplayerSessionMember* member = membersadded.at(i);
				if (stricmp(member->DeviceToken.Value, hostToken) == 0)
				{
					host = member;
					break;
				}
			}

			if (host == NULL)
			{
				XSM_VERBOSE_OUTPUT("Host not one of the newly added players : session id %d, session name %s\n", _session->id, GetSessionName(_session->session, stringbuff));
			}
			else
			{
				if (stricmp(host->DeviceToken.Value, currentuser->DeviceToken.Value) == 0)
				{
					// If the host is on the current machine, don't trigger a session joined event
					XSM_VERBOSE_OUTPUT("Host is on this machine - not reporting new connection : session id %d, session name %s\n", _session->id, GetSessionName(_session->session, stringbuff));
				}
				else
				{

					// Get entity ID from the member, and signal that they have joined	
					const XblMultiplayerSessionMember* member = host;
					const char* entityID = GetSessionMemberProperty(_updatedsession, member, "entityID");
					if (entityID == NULL)
					{
						// This user doesn't have a valid entity string in the session document, so it hasn't been fully set up yet						
						return;
					}					

					SignalMemberJoined(member->Xuid, entityID, _session, true);

					YYFree(entityID);

					XSM_VERBOSE_OUTPUT("Received host connection : session id %d, session name %s\n", _session->id, GetSessionName(_session->session, stringbuff));
				}

			}
		}
#endif

	}

#endif // ENFORCE_STAR_NETWORK_TOPOLOGY
}


void
XSM::OnSessionChanged(
	uint64 _user_id,
	xbl_session_ptr _updatedsession
	)
{

#ifndef WIN_UAP

	if (_user_id != 0 && _updatedsession != NULL)
		DebugConsoleOutput("OnSessionChanged called for user %lld session id %s\n", _user_id, XblMultiplayerSessionSessionReference(_updatedsession->session_handle)->SessionName);
	else
	{
		if (_user_id != 0)
		{
			DebugConsoleOutput("OnSessionChanged called for user %lld session id\n", _user_id);
		}
		else if (_updatedsession != NULL)
		{
			DebugConsoleOutput("OnSessionChanged called session id %s\n", XblMultiplayerSessionSessionReference(_updatedsession->session_handle)->SessionName);

		}
		else
		{
			DebugConsoleOutput("OnSessionChanged called with null user and session");

		}

	}
#endif
	XSM_LOCK_MUTEX

	// Iterate through the task list and do work based on the type and state of each task object
	size_t count = tasks->size();

	bool dbg_processed = false;
	size_t i;
	for(i = 0; i < count; i++)
	{
		XSMtaskBase* xsmtask = tasks->at(i);

		if (xsmtask != NULL)
		{
			// First compare user which 'initiated' this event to avoid processing events multiple times if we have multiple event handlers hooked up to different users
			if ((xsmtask->state != XSMTS_Finished) && ((xsmtask->user_id == _user_id)
				|| (_user_id == 0)))		// if we don't specify a user, just process all contexts (most likely means that this function isn't being called from an event handler)
			{
				// Check that this event applies to the session associated with this context
				if (AreSessionsEqual(_updatedsession, xsmtask->session, false))
				{
#ifdef XSM_VERBOSE_TRACE
					// Compare the old and new sessions
					XblMultiplayerSessionChangeTypes changes = XblMultiplayerSessionCompare(_updatedsession->session_handle, xsmtask->session->session_handle);
					DebugConsoleOutput("Session change 0x%08x, task type %s, state %d, request id %d\n", (uint32)changes, g_XSMTaskNames[xsmtask->taskType], xsmtask->state, xsmtask->requestid);
#endif
					// The ProcessSessionChanged functions can't change any associated XSMsession objects otherwise it'll muck up the change detection below
					xsmtask->ProcessSessionChanged(_updatedsession);

					dbg_processed = true;
				}
			}
		}
	}
	
	XSMsession* xsmsession = GetSession(_updatedsession);
	if (xsmsession != nullptr)
	{	
		// Right, check for different conditions - TBD

		if (xsmsession->session != nullptr)
		{
			// Compare the old and new sessions
			XblMultiplayerSessionChangeTypes changes = XblMultiplayerSessionCompare( _updatedsession->session_handle, xsmsession->session->session_handle );

			// Checks for player joined\left
			if (((changes & XblMultiplayerSessionChangeTypes::MemberListChange) == XblMultiplayerSessionChangeTypes::MemberListChange)
			 || ((changes & XblMultiplayerSessionChangeTypes::MemberStatusChange) == XblMultiplayerSessionChangeTypes::MemberStatusChange)
			 || ((changes & XblMultiplayerSessionChangeTypes::CustomPropertyChange) == XblMultiplayerSessionChangeTypes::CustomPropertyChange)
			 || ((changes & XblMultiplayerSessionChangeTypes::MemberCustomPropertyChange) == XblMultiplayerSessionChangeTypes::MemberCustomPropertyChange)) // This happens when a member completes PlayFab network authentication and modifies the session document
			{				
				ProcessMemberListChanged(xsmsession, _updatedsession);
			}
		}


		// Finally, update the session object
		xsmsession->session = _updatedsession;	
	}
}


void
XSM::OnSessionChanged(
	uint64 _user_id,
	XblMultiplayerSessionChangeEventArgs args
    )
{		
#ifdef QUEUE_CHANGED_EVENTS

	XSM_LOCK_EVENT_MUTEX

	SessionChangedRecord* newRecord = new SessionChangedRecord();
	newRecord->user_id = _user_id;
	newRecord->args = args;

	sessionChangedRecords->push_back(newRecord);		
#endif
}

void XSM::OnPlayFabPartyXblChange(
	const Party::PartyXblStateChange* _change
)
{
	// This should happen on the main thread so we don't need to queue anything up

	XSM_LOCK_MUTEX;

	// Iterate through the task list and dispatch this change to each of them
	// Don't do any filtering based on session etc as not all the change structures include them - let the tasks sort them out
	size_t count = tasks->size();

	size_t i;
	for (i = 0; i < count; i++)
	{
		XSMtaskBase* xsmtask = tasks->at(i);

		if (xsmtask != NULL)
		{
			if (xsmtask->state != XSMTS_Finished)
			{
				// The ProcessSessionChanged functions can't change any associated XSMsession objects otherwise it'll muck up the change detection below
				xsmtask->ProcessPlayFabPartyXblChange(_change);
			}
		}
	}
}

void XSM::OnPlayFabPartyChange(
	const Party::PartyStateChange* _change
)
{
	// This should happen on the main thread so we don't need to queue anything up

	XSM_LOCK_MUTEX;

	// Iterate through the task list and dispatch this change to each of them
	// Don't do any filtering based on session etc as not all the change structures include them - let the tasks sort them out
	size_t count = tasks->size();

	size_t i;
	for (i = 0; i < count; i++)
	{
		XSMtaskBase* xsmtask = tasks->at(i);

		if (xsmtask != NULL)
		{			
			if (xsmtask->state != XSMTS_Finished)
			{
				// The ProcessSessionChanged functions can't change any associated XSMsession objects otherwise it'll muck up the change detection below
				xsmtask->ProcessPlayFabPartyChange(_change);
			}
		}
	}
}

void XSMtaskBase::Process()
{
	// Some common stuff
	HRESULT res = S_OK;
	XUMuser* pUser = XUM::GetUserFromId(user_id);
	XblContextHandle xbl_context = NULL;
	if (pUser != NULL)
	{
		xbl_context = pUser->GetXboxLiveContext();
	}

	if (xbl_context == NULL)
	{
		SetState(XSMTS_FailureCleanup);
		return;
	}

	switch (state)
	{
		case XSMTS_SetupPlayFabNetwork:
		{
			// First wait for the local user to be logged into the playfab system if they haven't been already
			// This process should have been kicked off when matchmaking was started (should we simply block any attempt to create sessions etc until the user is logged in?)
			// If their current state is logged-out, this means that the log-in attempt failed (or could they be kicked out? It seems not from the documentation)
			// We could either retry or just bail
			if (pUser->playfabState == XUMuserPlayFabState::logging_in)
			{
				return;			// wait until the player is logged in (TODO: have a timeout in case things get wacky and the user sticks in this state)
			}

			if (pUser->playfabState != XUMuserPlayFabState::logged_in)
			{
				// If they're not logged in, something's gone wrong, so fail and bail
				SetState(XSMTS_FailureCleanup);
				return;
			}

			const XblMultiplayerSessionConstants* consts = XblMultiplayerSessionSessionConstants(session->session_handle);
			if (consts == NULL)
			{
				// Err
				SetState(XSMTS_FailureCleanup);
				return;
			}

			XSMPlayFabPayload* payload = new XSMPlayFabPayload();
			payload->user_id = user_id;
			payload->session = session;
			payload->requestid = requestid;

			if (PlayFabPartyManager::CreateNetwork(user_id, consts->MaxMembersInSession, payload) < 0)
			{
				delete payload;

				SetState(XSMTS_FailureCleanup);
				return;
			}

			SetState(XSMTS_WaitForPlayFabNetworkSetup);
			waiting = true;

		} break;

		case XSMTS_WritePlayFabNetworkDetailsToSession:
		{
			// Write custom properties to the session so they can be read by other players
			char descriptorString[c_maxSerializedNetworkDescriptorStringLength + 1];
			if (PlayFabPartyManager::SerialiseNetworkDescriptor(&networkDescriptor, descriptorString) < 0)
			{
				XSM_VERBOSE_OUTPUT("(XSMTS_WritePlayFabDetailsToSession) couldn't serialise network descriptor: request id %d\n", requestid);
				SetState(XSMTS_FailureCleanup);
				return;
			}

			res = SetSessionProperty("networkdescriptor", descriptorString);
			if (FAILED(res))
			{
				XSM_VERBOSE_OUTPUT("(XSMTS_WritePlayFabDetailsToSession) couldn't write session property: request id %d\n", requestid);
				SetState(XSMTS_FailureCleanup);
				return;
			}

			res = SetSessionProperty("invite", invitationIdentifier);
			if (FAILED(res))
			{
				XSM_VERBOSE_OUTPUT("(XSMTS_WritePlayFabDetailsToSession) couldn't write session property: request id %d\n", requestid);
				SetState(XSMTS_FailureCleanup);
				return;
			}

#if 0
			if ((pUser == NULL) || (pUser->playfabLocalUser == NULL))
			{
				XSM_VERBOSE_OUTPUT("(XSMTS_WritePlayFabDetailsToSession): error: PlayFab local user was NULL\n");
				res = E_FAIL;

				SetState(XSMTS_FailureCleanup);
			}
			else
			{
				// Write entity ID to the session so other users in the session can correlate Xuids with our PlayFab entity ID
				const char* entityID = PlayFabPartyManager::GetUserEntityID(user_id);
				if (entityID == NULL)
				{
					XSM_VERBOSE_OUTPUT("(XSMTS_WritePlayFabDetailsToSession): error: Couldn't get user's entity ID\n");
					res = E_FAIL;

					SetState(XSMTS_FailureCleanup);
				}
				else
				{
					char* xuidstring = XuidToString(pUser->XboxUserIdInt);

					if (SetSessionProperty(xuidstring, entityID) < 0)
					{
						XSM_VERBOSE_OUTPUT("(XSMTS_WritePlayFabDetailsToSession): error: Couldn't set session property\n");
						res = E_FAIL;

						SetState(XSMTS_FailureCleanup);
					}

					YYFree(xuidstring);
				}
			}
#endif

			res = WriteSessionAsync(xbl_context, session->session_handle, XblMultiplayerSessionWriteMode::UpdateExisting,
				[](XAsyncBlock* asyncBlock)
				{
					XSMtaskBase* self = (XSMtaskBase*)(asyncBlock->context);

					if (self->state == XSMTS_Finished)
					{
						// This task has been terminated, so do nothing						
					}
					else
					{
						XblMultiplayerSessionHandle sessionHandle;
						HRESULT hr = XblMultiplayerWriteSessionResult(asyncBlock, &sessionHandle);
						if (SUCCEEDED(hr))
						{
							// Process multiplayer session handle
							xbl_session_ptr session = std::make_shared<xbl_session>(sessionHandle);
							XSM_VERBOSE_OUTPUT("(XSMTS_WritePlayFabDetailsToSession) write succeeded: request id %d\n", self->requestid);
							self->SetState(XSMTS_ConnectToPlayFabNetwork);

							XSM::OnSessionChanged(0, session);

							self->waiting = false;
						}
						else
						{
							// Handle failure
							XSM_VERBOSE_OUTPUT("(XSMTS_WritePlayFabDetailsToSession) write failed: request id %d\n", self->requestid);
							self->SetState(XSMTS_FailureCleanup);
							self->waiting = false;
						}
					}

					delete asyncBlock;
				});

			if (SUCCEEDED(res))
			{
				waiting = true;	// set to waiting
			}
			else
			{
				DebugConsoleOutput("(XSMTS_WritePlayFabDetailsToSession) write failed: request id %d\n", requestid);

				SetState(XSMTS_FailureCleanup);
				waiting = false;
			}
		} break;

		case XSMTS_GetPlayFabNetworkDetailsAndConnect:
		{			
			const char* temp_networkdescriptor = GetSessionProperty("networkdescriptor");
			const char* temp_invite = GetSessionProperty("invite");

			if ((temp_networkdescriptor == NULL) || (temp_invite == NULL))
			{
				// Poll these properties until they're available or we timeout								
				int64 currtime = Timing_Time();
				if ((currtime - state_starttime) > PLAYFAB_NETWORK_DETAILS_TIMEOUT)
				{
					if (temp_networkdescriptor == NULL)
					{
						XSM_VERBOSE_OUTPUT("(XSMTS_GetPlayFabNetworkDetailsAndConnect) couldn't get network descriptor from session: request id %d\n", requestid);
					}

					if (temp_invite == NULL)
					{
						XSM_VERBOSE_OUTPUT("(XSMTS_GetPlayFabNetworkDetailsAndConnect) couldn't get invitation from session: request id %d\n", requestid);
					}

					SetState(XSMTS_FailureCleanup);					
				}

				YYFree(temp_networkdescriptor);
				YYFree(temp_invite);
				return;
			}

			// Wait for the local user to be logged into the playfab system if they haven't been already
			// This process should have been kicked off when matchmaking was started (should we simply block any attempt to create/join sessions etc until the user is logged in?)
			// If their current state is logged-out, this means that the log-in attempt failed (or could they be kicked out? It seems not from the documentation)
			// We could either retry or just bail
			if (pUser->playfabState == XUMuserPlayFabState::logging_in)
			{
				return;			// wait until the player is logged in (TODO: have a timeout in case things get wacky and the user sticks in this state)
			}

			if (pUser->playfabState != XUMuserPlayFabState::logged_in)
			{
				// If they're not logged in, something's gone wrong, so fail and bail
				SetState(XSMTS_FailureCleanup);
				return;
			}

			if (pUser->playfabLocalUser == NULL)
			{
				// If there's no local user, something's gone wrong, so fail and bail
				SetState(XSMTS_FailureCleanup);
				return;
			}

			if (PlayFabPartyManager::DeserialiseNetworkDescriptor(temp_networkdescriptor, &networkDescriptor) < 0)
			{
				XSM_VERBOSE_OUTPUT("(XSMTS_GetPlayFabNetworkDetailsAndConnect) couldn't deserialise network descriptor: request id %d\n", requestid);

				YYFree(temp_networkdescriptor);
				YYFree(temp_invite);
				SetState(XSMTS_FailureCleanup);
				return;
			}

			strcpy(invitationIdentifier, temp_invite);

			YYFree(temp_networkdescriptor);
			YYFree(temp_invite);

			// Now that we have all the details, connect to the network
			SetState(XSMTS_ConnectToPlayFabNetwork);
		} break;

		case XSMTS_ConnectToPlayFabNetwork:
		{			
			// First wait for the local user to be logged into the playfab system if they haven't been already
			// This process should have been kicked off when matchmaking was started (should we simply block any attempt to create/join sessions etc until the user is logged in?)
			// If their current state is logged-out, this means that the log-in attempt failed (or could they be kicked out? It seems not from the documentation)
			// We could either retry or just bail
			if (pUser->playfabState == XUMuserPlayFabState::logging_in)
			{
				return;			// wait until the player is logged in (TODO: have a timeout in case things get wacky and the user sticks in this state)
			}

			if (pUser->playfabState != XUMuserPlayFabState::logged_in)
			{
				// If they're not logged in, something's gone wrong, so fail and bail
				SetState(XSMTS_FailureCleanup);
				return;
			}

			if (pUser->playfabLocalUser == NULL)
			{
				// If there's no local user, something's gone wrong, so fail and bail
				SetState(XSMTS_FailureCleanup);
				return;
			}

			// Make sure the session has up-to-date network info
			XSMsession* xsmsession = XSM::GetSession(session);
			//xsmsession->network = network;
			strcpy(xsmsession->playfabNetworkIdentifier, networkDescriptor.networkIdentifier);

			// The main task code should have stored the network descriptor and invitation before switching to this state
			XSMPlayFabPayload* payload = new XSMPlayFabPayload();
			payload->user_id = user_id;
			payload->session = session;
			payload->requestid = requestid;

			if (PlayFabPartyManager::ConnectToNetwork(&networkDescriptor, payload) < 0)
			{
				delete payload;

				SetState(XSMTS_FailureCleanup);
			}
			else
			{
				SetState(XSMTS_WaitForConnectToPlayFabNetwork);
				waiting = true;
			}
		} break;

		case XSMTS_WriteUserPlayFabDetails:
		{
			if ((pUser == NULL) || (pUser->playfabLocalUser == NULL))
			{
				XSM_VERBOSE_OUTPUT("(XSMTS_WritePlayFabDetailsToSession): error: PlayFab local user was NULL\n");
				res = E_FAIL;

				SetState(XSMTS_FailureCleanup);
			}
			else
			{
				// Write entity ID to the session so other users in the session can correlate Xuids with our PlayFab entity ID
				const char* entityID = PlayFabPartyManager::GetUserEntityID(user_id);
				if (entityID == NULL)
				{
					XSM_VERBOSE_OUTPUT("(XSMTS_WritePlayFabDetailsToSession): error: Couldn't get user's entity ID\n");
					res = E_FAIL;

					SetState(XSMTS_FailureCleanup);
				}
				else
				{
					//char* xuidstring = XuidToString(pUser->XboxUserIdInt);

					//if (SetSessionProperty(xuidstring, entityID) < 0)
					if (SetSessionCurrentUserProperty("entityID", entityID) < 0)
					{
						XSM_VERBOSE_OUTPUT("(XSMTS_WritePlayFabDetailsToSession): error: Couldn't set session property\n");
						res = E_FAIL;

						SetState(XSMTS_FailureCleanup);
					}

					//YYFree(xuidstring);
				}
			}

			if (SUCCEEDED(res))
			{
				res = WriteSessionAsync(xbl_context, session->session_handle, XblMultiplayerSessionWriteMode::UpdateExisting,
					[](XAsyncBlock* asyncBlock)
					{
						XSMtaskBase* self = (XSMtaskBase*)(asyncBlock->context);

						if (self->state == XSMTS_Finished)
						{
							// This task has been terminated, so do nothing						
						}
						else
						{
							XblMultiplayerSessionHandle sessionHandle;
							HRESULT hr = XblMultiplayerWriteSessionResult(asyncBlock, &sessionHandle);
							if (SUCCEEDED(hr))
							{
								// Process multiplayer session handle
								xbl_session_ptr session = std::make_shared<xbl_session>(sessionHandle);
								XSM_VERBOSE_OUTPUT("(XSMTS_WritePlayFabDetailsToSession) write succeeded: request id %d\n", self->requestid);
								self->SetState(XSMTS_ConnectionToPlayFabNetworkCompleted);

								XSM::OnSessionChanged(0, session);

								self->waiting = false;
							}
							else
							{
								// Handle failure
								XSM_VERBOSE_OUTPUT("(XSMTS_WritePlayFabDetailsToSession) write failed: request id %d\n", self->requestid);
								self->SetState(XSMTS_FailureCleanup);
								self->waiting = false;
							}
						}

						delete asyncBlock;
					});

				if (SUCCEEDED(res))
				{
					waiting = true;	// set to waiting
				}
				else
				{
					XSM_VERBOSE_OUTPUT("(XSMTS_WritePlayFabDetailsToSession) write failed: request id %d\n", requestid);

					SetState(XSMTS_FailureCleanup);
					waiting = false;
				}
			}
		} break;
	};
}

void XSMtaskBase::ProcessPlayFabPartyChange(const PartyStateChange* _change)
{
	if (_change == NULL)
		return;

	switch (state)
	{
		case XSMTS_WaitForPlayFabNetworkSetup:
		{
			if (_change->stateChangeType == PartyStateChangeType::CreateNewNetworkCompleted)
			{
				const PartyCreateNewNetworkCompletedStateChange* result = static_cast<const PartyCreateNewNetworkCompletedStateChange*>(_change);
				if (result)
				{
					XSMPlayFabPayload* payload = NULL;
					if (!IsPayloadOfType(result->asyncIdentifier, PlayFabPayloadType::XSM))
						return;

					payload = (XSMPlayFabPayload*)(result->asyncIdentifier);
					if (!IsXSMPlayFabPayloadForUs(payload))
						return;

					if (result->result == PartyStateChangeResult::Succeeded)
					{

						memcpy(&(networkDescriptor), &(result->networkDescriptor), sizeof(PartyNetworkDescriptor));
						if (result->appliedInitialInvitationIdentifier != NULL)
						{
							strcpy(invitationIdentifier, result->appliedInitialInvitationIdentifier);
						}
						else
						{
							invitationIdentifier[0] = '\0';
						}

						// Write network details to session
						SetState(XSMTS_WritePlayFabNetworkDetailsToSession);
						waiting = false;
					}
					else
					{
						XSM_VERBOSE_OUTPUT("(XSMTS_WaitForPlayFabNetworkSetup) couldn't create network: request id %d\n", requestid);
						SetState(XSMTS_FailureCleanup);
						waiting = false;
					}
				}
			}
		} break;

		case XSMTS_WaitForConnectToPlayFabNetwork:
		{
			if (_change->stateChangeType == PartyStateChangeType::ConnectToNetworkCompleted)
			{
				const PartyConnectToNetworkCompletedStateChange* result = static_cast<const PartyConnectToNetworkCompletedStateChange*>(_change);
				if (result)
				{
					XSMPlayFabPayload* payload = NULL;
					if (!IsPayloadOfType(result->asyncIdentifier, PlayFabPayloadType::XSM))
						return;

					payload = (XSMPlayFabPayload*)(result->asyncIdentifier);
					if (!IsXSMPlayFabPayloadForUs(payload))
						return;

					if (result->result == PartyStateChangeResult::Succeeded)
					{
						network = result->network;

						// Now lets try authenticating the user for this task

						// Copy the payload (as the original one will be deleted in the PlayFabPartyManager event handler)						
						XSMPlayFabPayload* newpayload = new XSMPlayFabPayload(*payload);

						if (PlayFabPartyManager::AuthenticateLocalUser(network, user_id, invitationIdentifier, newpayload) < 0)
						{
							delete newpayload;

							SetState(XSMTS_FailureCleanup);
							waiting = false;
						}
						else
						{
							SetState(XSMTS_WaitForAuthenticateLocalUser);
						}
					}
					else
					{
						XSM_VERBOSE_OUTPUT("(XSMTS_WaitForConnectToPlayFabNetwork) couldn't connect to network: request id %d\n", requestid);
						SetState(XSMTS_FailureCleanup);
						waiting = false;
					}
				}
			}
		} break;

		case XSMTS_WaitForAuthenticateLocalUser:
		{
			if (_change->stateChangeType == PartyStateChangeType::AuthenticateLocalUserCompleted)
			{
				const PartyAuthenticateLocalUserCompletedStateChange* result = static_cast<const PartyAuthenticateLocalUserCompletedStateChange*>(_change);
				if (result)
				{
					XSMPlayFabPayload* payload = NULL;
					if (!IsPayloadOfType(result->asyncIdentifier, PlayFabPayloadType::XSM))
						return;

					payload = (XSMPlayFabPayload*)(result->asyncIdentifier);
					if (!IsXSMPlayFabPayloadForUs(payload))
						return;

					if (result->result == PartyStateChangeResult::Succeeded)
					{
						// Connect local device's chat control to the network

						// Copy the payload (as the original one will be deleted in the PlayFabPartyManager event handler)						
						XSMPlayFabPayload* newpayload = new XSMPlayFabPayload(*payload);

						if (PlayFabPartyManager::ConnectChatControl(network, user_id, newpayload) < 0)
						{
							delete newpayload;

							SetState(XSMTS_FailureCleanup);
							waiting = false;
						}
						else
						{
							SetState(XSMTS_WaitForConnectChatControl);
						}
					}
					else
					{
						XSM_VERBOSE_OUTPUT("(XSMTS_WaitForAuthenticateLocalUser) couldn't authenticate local user: request id %d\n", requestid);
						SetState(XSMTS_FailureCleanup);
						waiting = false;
					}
				}
			}
		} break;

		case XSMTS_WaitForConnectChatControl:
		{
			if (_change->stateChangeType == PartyStateChangeType::ConnectChatControlCompleted)
			{
				const PartyConnectChatControlCompletedStateChange* result = static_cast<const PartyConnectChatControlCompletedStateChange*>(_change);
				if (result)
				{
					XSMPlayFabPayload* payload = NULL;
					if (!IsPayloadOfType(result->asyncIdentifier, PlayFabPayloadType::XSM))
						return;

					payload = (XSMPlayFabPayload*)(result->asyncIdentifier);
					if (!IsXSMPlayFabPayloadForUs(payload))
						return;

					if (result->result == PartyStateChangeResult::Succeeded)
					{
						// Creating the endpoint

						// Copy the payload (as the original one will be deleted in the PlayFabPartyManager event handler)						
						XSMPlayFabPayload* newpayload = new XSMPlayFabPayload(*payload);

						if (PlayFabPartyManager::CreateEndpoint(network, user_id, newpayload) < 0)
						{
							delete newpayload;

							SetState(XSMTS_FailureCleanup);
							waiting = false;
						}
						else
						{
							SetState(XSMTS_WaitForCreateEndpoint);
						}
					}
					else
					{
						XSM_VERBOSE_OUTPUT("(XSMTS_WaitForConnectChatControl) couldn't connect chat control: request id %d\n", requestid);
						SetState(XSMTS_FailureCleanup);
						waiting = false;
					}
				}
			}
		} break;

		case XSMTS_WaitForCreateEndpoint:
		{
			if (_change->stateChangeType == PartyStateChangeType::CreateEndpointCompleted)
			{
				const PartyCreateEndpointCompletedStateChange* result = static_cast<const PartyCreateEndpointCompletedStateChange*>(_change);
				if (result)
				{
					XSMPlayFabPayload* payload = NULL;
					if (!IsPayloadOfType(result->asyncIdentifier, PlayFabPayloadType::XSM))
						return;

					payload = (XSMPlayFabPayload*)(result->asyncIdentifier);
					if (!IsXSMPlayFabPayloadForUs(payload))
						return;

					if (result->result == PartyStateChangeResult::Succeeded)
					{
						// We've successfully created the endpoint
						// We won't store it as that means less to manage

						// We've completed setup of this user, so now write the user details to the session
						SetState(XSMTS_WriteUserPlayFabDetails);
						waiting = false;
					}
					else
					{
						XSM_VERBOSE_OUTPUT("(XSMTS_WaitForCreateEndpoint) couldn't create endpoint: request id %d\n", requestid);
						SetState(XSMTS_FailureCleanup);
						waiting = false;
					}
				}
			}
		} break;

		default: break;
	}
}

void XSMtaskBase::SetState(int _newstate)
{
	state = _newstate;
	state_starttime = Timing_Time();
}

HRESULT XSMtaskBase::WriteSessionAsync(XblContextHandle xblContext,
	XblMultiplayerSessionHandle multiplayerSession,
	XblMultiplayerSessionWriteMode writeMode,
	XAsyncCompletionRoutine* func,
	const char* optionalHandle)
{
	HRESULT res;
	XAsyncBlock* asyncBlock = new XAsyncBlock();
	asyncBlock->queue = XSM::GetTaskQueue();
	asyncBlock->context = this;
	asyncBlock->callback = func;

	if (optionalHandle == NULL)
		res = XblMultiplayerWriteSessionAsync(xblContext, multiplayerSession, writeMode, asyncBlock);
	else
		res = XblMultiplayerWriteSessionByHandleAsync(xblContext, multiplayerSession, writeMode, optionalHandle, asyncBlock);
	if (FAILED(res))
	{
		delete asyncBlock;
	}

	return res;
}

HRESULT XSMtaskBase::SetSessionProperty(const char* _name, const char* _value)
{
	return ::SetSessionProperty(session, _name, _value);	
}

HRESULT XSMtaskBase::SetSessionCurrentUserProperty(const char* _name, const char* _value)
{
	return ::SetSessionCurrentUserProperty(session, _name, _value);
}

const char* XSMtaskBase::GetSessionProperty(const char* _name)
{
	return ::GetSessionProperty(session, _name);
}

const char* XSMtaskBase::GetSessionMemberProperty(const XblMultiplayerSessionMember* _member, const char* _name)
{
	return ::GetSessionMemberProperty(session, _member, _name);
}

const char* XSMtaskBase::GetSessionUserProperty(uint64 _user_id, const char* _name)
{
	return ::GetSessionUserProperty(session, _user_id, _name);
}

HRESULT XSMtaskBase::DeleteSessionProperty(const char* _name)
{
	return ::DeleteSessionProperty(session, _name);
}

HRESULT XSMtaskBase::DeleteMatchTicketAsync(XblContextHandle xboxLiveContext,
	const char* serviceConfigurationId,
	const char* hopperName,
	const char* ticketId)
{
	XAsyncBlock* asyncBlock = new XAsyncBlock();
	asyncBlock->queue = XSM::GetTaskQueue();
	asyncBlock->context = this;
	asyncBlock->callback = [](XAsyncBlock* _asyncBlock)
	{
		// Fire-and-forget - we don't care about the result of this
		delete _asyncBlock;
	};

	HRESULT res = XblMatchmakingDeleteMatchTicketAsync(xboxLiveContext, serviceConfigurationId, hopperName, ticketId, asyncBlock);
	if (FAILED(res))
	{
		delete asyncBlock;
	}

	return res;
}

bool XSMtaskBase::IsXSMPlayFabPayloadForUs(XSMPlayFabPayload* _payload)
{
	if (_payload == NULL)
		return false;

	if ((_payload->user_id == user_id) || (_payload->user_id == 0))
	{
		// Check that this event applies to the session associated with this context
		if (AreSessionsEqual(_payload->session, session, false))
		{
			return true;
		}
	}

	return false;
}

char* XuidToString(uint64 _xuid)
{
	char* out = (char*)YYAlloc(64 * sizeof(char));		// just has to be big enough
	snprintf(out, 64, "%lld", _xuid);

	return out;
}
