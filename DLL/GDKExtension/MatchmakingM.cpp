//
// Copyright (C) 2020 Opera Norway AS. All rights reserved.
//
// This file is an original work developed by Opera.
//

#include "pch.h"

#include <inttypes.h>
#include <json-c/json.h>
#include <objbase.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <XGameUI.h>
#include <xsapi-c/services_c.h>
#include <XUser.h>

#include "GDKX.h"
#include "CreateSession.h"
#include "SessionManagement.h"
#include "UserManagement.h"


extern int GetGamepadIndex(const APP_LOCAL_DEVICE_ID* _id);
extern APP_LOCAL_DEVICE_ID GetGamepadDeviceID(int _index);

#include <Windows.h>
#include <ppltasks.h>

#if !defined(WIN_UAP) && !defined(NO_SECURE_CONNECTION) && YY_CHAT
#include "Multiplayer/GameChat2IntegrationLayer.h"
#endif

extern int JSONToDSMap(char* _jsonData, int _map);
extern int JSONToDSList(char* _jsonListData, char* _key, int _list);

extern bool		g_SocketInitDone;
extern	int		g_IDE_Version;

void F_XboxOneMatchmakingCreate(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg);
void F_XboxOneMatchmakingFind(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg);
void F_XboxOneMatchmakingStart(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg);
void F_XboxOneMatchmakingStop(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg);
void F_XboxOneMatchmakingSessionGetUsers(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg);
void F_XboxOneMatchmakingSessionLeave(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg);
void F_XboxOneMatchmakingSendInvites(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg);
void F_XboxOneMatchmakingSetJoinableSession(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg);
void F_XboxOneMatchmakingJoinInvite(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg);
void F_XboxOneMatchmakingJoinSessionHandle(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg);

void F_XboxOneChatAddUserToChannel(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg);
void F_XboxOneChatRemoveUserFromChannel(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg);

void F_XboxOneChatSetMuted(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg);
void F_XboxOneChatGetMuted(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg);
void F_XboxOneChatAddUser(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg);
void F_XboxOneChatRemoveUser(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg);
void F_XboxOneChatSetCommunicationRelationship(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg);

void F_XboxOneSetServiceConfigurationID(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg);
void F_XboxOneSetFindSessionTimeout(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg);

void F_XboxLiveNotAvailable(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg);

void F_PlayFabSendPacket(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg);
void F_PlayFabSendReliablePacket(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg);
void F_PlayFabGetUserAddress(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg);


int g_findSessionTimeout = 60; //default 60 second timeout

extern bool g_chatIntegrationLayerInited;
extern bool g_XboxOneGameChatEnabled;

// #############################################################################################
/// Function:<summary>
///             Initialise the built in YoYo games functions
///          </summary>
// #############################################################################################
void InitMatchmakingFunctionsM( void )
{
#if 0
	Function_Add( "xboxone_matchmaking_create", F_XboxOneMatchmakingCreate, -1, true);
	Function_Add( "xboxone_matchmaking_find", F_XboxOneMatchmakingFind, -1, true);
	Function_Add( "xboxone_matchmaking_start", F_XboxOneMatchmakingStart, 1, true);
	Function_Add( "xboxone_matchmaking_stop", F_XboxOneMatchmakingStop, 1, true);
	Function_Add( "xboxone_matchmaking_session_get_users", F_XboxOneMatchmakingSessionGetUsers, 1, true);
	Function_Add( "xboxone_matchmaking_session_leave", F_XboxOneMatchmakingSessionLeave, 1, true);

	Function_Add( "xboxone_matchmaking_send_invites", F_XboxOneMatchmakingSendInvites, 3, true);
	Function_Add( "xboxone_matchmaking_set_joinable_session", F_XboxOneMatchmakingSetJoinableSession, 2, true);
	Function_Add( "xboxone_matchmaking_join_invite", F_XboxOneMatchmakingJoinInvite, 4, true);
	Function_Add( "xboxone_matchmaking_join_session", F_XboxOneMatchmakingJoinSessionHandle, 3, true);

	Function_Add( "xboxone_matchmaking_set_find_timeout", F_XboxOneSetFindSessionTimeout, 1, true);

	Function_Add( "xboxone_chat_add_user_to_channel", F_XboxOneChatAddUserToChannel, 2, true);
	Function_Add( "xboxone_chat_remove_user_from_channel", F_XboxOneChatRemoveUserFromChannel, 2, true);

	Function_Add( "xboxone_chat_set_muted", F_XboxOneChatSetMuted, 2, true);
	Function_Add( "xboxone_chat_get_muted", F_XboxOneChatGetMuted, 1, true);
	Function_Add( "xboxone_chat_add_user", F_XboxOneChatAddUser, 1, true);
	Function_Add( "xboxone_chat_remove_user", F_XboxOneChatRemoveUser, 1, true);
	Function_Add( "xboxone_chat_set_communication_relationship", F_XboxOneChatSetCommunicationRelationship, 3, true);

	Function_Add("playfab_send_packet", F_PlayFabSendPacket, 4, true);
	Function_Add("playfab_send_reliable_packet", F_PlayFabSendReliablePacket, 4, true);
	Function_Add("playfab_get_user_address", F_PlayFabGetUserAddress, 1, true);
#endif
}

// #############################################################################################
/// Function:<summary>
///             Initialise the platform level constants
///          </summary>
// #############################################################################################
void	InitMatchmakingConstantsM( void )
{
#if 0
	AddRealConstant("xboxone_visibility_open", 1);
	AddRealConstant("xboxone_visibility_private", 2);

	AddRealConstant("network_type_xboxone_connection_established", 32);	// comes after other network_type event values

	AddRealConstant("xboxone_match_visibility_usetemplate", 0);
    AddRealConstant("xboxone_match_visibility_open", 1);
    AddRealConstant("xboxone_match_visibility_private", 2);

	AddRealConstant("xboxone_chat_relationship_none", 0);
	AddRealConstant("xboxone_chat_relationship_receive_all", 24);
	AddRealConstant("xboxone_chat_relationship_receive_audio", 8);
	AddRealConstant("xboxone_chat_relationship_receive_text", 16);
	AddRealConstant("xboxone_chat_relationship_send_all", 7);
	AddRealConstant("xboxone_chat_relationship_send_and_receive_all", 31);
	AddRealConstant("xboxone_chat_relationship_send_microphone_audio", 1);
	AddRealConstant("xboxone_chat_relationship_send_text", 4);
	AddRealConstant("xboxone_chat_relationship_send_text_to_speech_audio", 2);

	AddRealConstant("network_type_xboxlive_connection_established", 32);	// comes after other network_type event values

#ifndef _GAMING_XBOX
	AddRealConstant("MATCHMAKING_SESSION", MATCHMAKING_SESSION);
	AddRealConstant("MATCHMAKING_INVITE", MATCHMAKING_INVITE);
#endif
#endif
}

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

// Matchmaking stuff
void F_XboxOneMatchmakingCreate(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_REAL;
	Result.val = -1;
	// Could technically move most of this setup into the main state machine for this operation
	// Doing it here though allows us to check for any initial failure cases straight away

	// Get user
	uint64 user_id = (uint64)YYGetInt64(arg, 0);

	XUM_LOCK_MUTEX
	XUMuser* user = XUM::GetUserFromId(user_id);

	if (user == NULL)
	{
		DebugConsoleOutput("xboxone_matchmaking_create() - error: user not found\n");
		return;
	}

	if (user->IsSignedIn == false)
	{
		DebugConsoleOutput("xboxone_matchmaking_create() - error: specified user not signed in\n");
		return;
	}

	DebugConsoleOutput("xboxone_matchmaking_create() called\n");


	// Generate visiblity value
	XblMultiplayerSessionVisibility visibility = XblMultiplayerSessionVisibility::Open;
	int argvis = YYGetInt32(arg, 1);
	switch(argvis)
	{
		case 0: visibility = XblMultiplayerSessionVisibility::Any; break;
		case 1: visibility = XblMultiplayerSessionVisibility::Open; break;
		case 2: visibility = XblMultiplayerSessionVisibility::PrivateSession; break;
	}
	
	//std::unique_ptr

	// Generate strings for template and hopper names
	const char* templatename = YYGetString(arg, 2);
	const char* hoppername = YYGetString( arg, 3);
	const char* sdatemplatename = YYGetString(arg, 4);

	const char* matchattributes = argc > 5 ? YYGetString(arg, 5) : "";

#ifdef NOV_XDK
	SecureDeviceAssociationTemplate^ assoctemplate = nullptr;
	try
	{
		assoctemplate = SecureDeviceAssociationTemplate::GetTemplateByName(sdatemplatename);
	}
	catch(Platform::Exception^ ex)
	{
		DebugConsoleOutput("xboxone_matchmaking_create() - error: secure device association template not recognised\n");
		return;
	}
#endif

	// Enable multiplayer subscriptions
	//user->EnableMultiplayerSubscriptions();

	// Generate new session name
	char sessionName[64];

	GUID sessionNameGUID;
    CoCreateGuid( &sessionNameGUID );
	GUIDtoString(&sessionNameGUID, sessionName);    
	//sessionName = Platform::String::Concat(sessionName, L"blah");

	XblContextHandle xboxLiveContext = user->GetXboxLiveContext();

	if(!xboxLiveContext)
	{
		DebugConsoleOutput("xboxone_matchmaking_create() - error: no xbox live context\n");
		return;
	}	

	XblMultiplayerSessionReference ref = {};
	// Make sure we copy one character less than the max to ensure that the last character is a null-terminator
	strncpy(ref.Scid, g_XboxSCID, sizeof(ref.Scid) - 1);
	strncpy(ref.SessionTemplateName, templatename, sizeof(ref.SessionTemplateName) - 1);
	strncpy(ref.SessionName, sessionName, sizeof(ref.SessionName) - 1);	

	XblMultiplayerSessionInitArgs args = {};
	//args.InitiatorXuids = &user_id;
	//args.InitiatorXuidsCount = 1;
	args.Visibility = visibility;

	XblMultiplayerSessionHandle sessionHandle = XblMultiplayerSessionCreateHandle(user_id, &ref, &args);
	xbl_session_ptr session(new xbl_session(sessionHandle));    

	// Quick test
	//const XblMultiplayerSessionProperties* props = XblMultiplayerSessionSessionProperties(session->session_handle);
	//if (props == NULL)
	//{
	//	printf("hello");
	//}

	HRESULT res = XblMultiplayerSessionJoin(
		sessionHandle,
		NULL,
		true,
		false);

	if (FAILED(res))
	{
		// The created session will be cleaned up by the smart pointer destruction
		DebugConsoleOutput("xboxone_matchmaking_create() - error: couldn't join created session (XblMultiplayerSessionJoin failed with 0x%08x)\n", res);
		return;
	}



	res = XblMultiplayerSessionCurrentUserSetStatus(
		sessionHandle,
		XblMultiplayerSessionMemberStatus::Active);	// a bit redundant, as this could be set in the XblMultiplayerSessionJoin call above, but this is what the XDK ver does

	if (FAILED(res))
	{
		// The created session will be cleaned up by the smart pointer destruction
		DebugConsoleOutput("xboxone_matchmaking_create() - error: couldn't set user status to active (XblMultiplayerSessionCurrentUserSetStatus failed with 0x%08x)\n", res);
		return;
	}
	
	// TODO: figure out if this is something we have to fill in with the Playfab Party stuff
//	Platform::String^ secureDeviceAddress = SecureDeviceAddress::GetLocal()->GetBase64String();
//
//#if 0
//	// Quick test - can we decode the secure device address?
//	Windows::Xbox::Networking::SecureDeviceAddress^ addr = Windows::Xbox::Networking::SecureDeviceAddress::GetLocal();
//	extern void base64_decode(const char* _encoded_string, size_t _out_len, char* _pBuffer, bool _add_null);
//	char *pBASE64 = ConvertFromWideCharToUTF8((wchar_t*)(secureDeviceAddress->Data()));
//	int src_len = strlen( pBASE64 );
//	int filesize = ((src_len*3)/4) + 4;
//	char* pFileBuffer = (char*)YYAlloc( filesize,__FILE__,__LINE__  );
//	base64_decode( pBASE64, filesize, pFileBuffer, false );
//#endif	
//
//#ifndef NO_SECURE_CONNECTION
//	session->SetCurrentUserSecureDeviceAddressBase64( secureDeviceAddress );
//#endif
	res = XblMultiplayerSessionCurrentUserSetSecureDeviceAddressBase64(sessionHandle, XSM::GetDeviceAddress()->value);
	if (FAILED(res))
	{
		// The created session will be cleaned up by the smart pointer destruction
		DebugConsoleOutput("xboxone_matchmaking_create() - error: couldn't set secure device address (XblMultiplayerSessionCurrentUserSetSecureDeviceAddressBase64 failed with 0x%08x)\n", res);
		return;
	}

	res = XblMultiplayerSessionSetSessionChangeSubscription(
		sessionHandle,
		XblMultiplayerSessionChangeTypes::Everything);	
	if (FAILED(res))
	{
		// The created session will be cleaned up by the smart pointer destruction
		DebugConsoleOutput("xboxone_matchmaking_create() - error: couldn't set session change subscription (XblMultiplayerSessionSetSessionChangeSubscription failed with 0x%08x)\n", res);
		return;
	}

#ifdef NO_SECURE_CONNECTION
	SetLocalIP(session);
#endif


	// Create and add a new task
	XSMtaskCreateSession* createsessiontask = new XSMtaskCreateSession();
	createsessiontask->session = session;
	createsessiontask->user_id = user_id;	
	createsessiontask->hopperName = (char*)YYStrDup(hoppername);
	createsessiontask->matchAttributes = (char*)YYStrDup(matchattributes);
	createsessiontask->requestid = XSM::GetNextRequestID();
	createsessiontask->SetState(XSMTS_CreateSession_InitialSession);

	XSM::AddTask(createsessiontask);

	// Add session to our tracked list	
	XSM::AddSession(session/*, assoctemplate*/, hoppername, matchattributes);
		
	Result.val = createsessiontask->requestid;

}

void F_XboxOneMatchmakingFind(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_REAL;
	Result.val = -1;

	// Get user	
	uint64 user_id = (uint64)YYGetInt64(arg, 0);

	XUM_LOCK_MUTEX
		XUMuser* user = XUM::GetUserFromId(user_id);

	if (user == NULL)
	{
		DebugConsoleOutput("xboxone_matchmaking_find() - error: user not found\n");
		return;
	}

	if (user->IsSignedIn == false)
	{
		DebugConsoleOutput("xboxone_matchmaking_find() - error: specified user not signed in\n");
		return;
	}

	// Generate strings for template and hopper names
	const char* templatename = YYGetString(arg, 1);
	const char* hoppername = YYGetString(arg, 2);
	const char* sdatemplatename = YYGetString(arg, 3);

	const char* matchattributes = (argc > 4) ? YYGetString(arg, 4) : "";
	
#ifdef NOV_XDK
	SecureDeviceAssociationTemplate^ assoctemplate = nullptr;
	try
	{
		assoctemplate = SecureDeviceAssociationTemplate::GetTemplateByName(sdatemplatename);
	}
	catch(Platform::Exception^ ex)
	{
		DebugConsoleOutput("xboxone_matchmaking_find() - error: secure device association template not recognised\n");
		return;
	}
#endif
	DebugConsoleOutput("xboxone_matchmaking_find() called\n");
	// temp
	//Platform::String^ templatename = ConvertCharArrayToManagedString( "MatchTicketSession" );
	//Platform::String^ hoppername = ConvertCharArrayToManagedString( "MatchTicketHopper" );

	// Enable multiplayer subscriptions
	//user->EnableMultiplayerSubscriptions();

	// Generate new session name
	char sessionName[64];

	GUID sessionNameGUID;
	CoCreateGuid(&sessionNameGUID);
	GUIDtoString(&sessionNameGUID, sessionName);
	//sessionName = Platform::String::Concat(sessionName, L"blah");

	XblContextHandle xboxLiveContext = user->GetXboxLiveContext();

	if(!xboxLiveContext)
	{
		DebugConsoleOutput("xboxone_matchmaking_find() - error: no xbox live context\n");
		return;
	}

	XblMultiplayerSessionReference ref = {};
	// Make sure we copy one character less than the max to ensure that the last character is a null-terminator
	strncpy(ref.Scid, g_XboxSCID, sizeof(ref.Scid) - 1);
	strncpy(ref.SessionTemplateName, templatename, sizeof(ref.SessionTemplateName) - 1);
	strncpy(ref.SessionName, sessionName, sizeof(ref.SessionName) - 1);

	XblMultiplayerSessionInitArgs args = {};
	//args.InitiatorXuids = &user_id;
	//args.InitiatorXuidsCount = 1;
	args.Visibility = XblMultiplayerSessionVisibility::Any;

	XblMultiplayerSessionHandle sessionHandle = XblMultiplayerSessionCreateHandle(user_id, &ref, &args);
	xbl_session_ptr session(new xbl_session(sessionHandle));

	HRESULT res = XblMultiplayerSessionJoin(
		sessionHandle,
		NULL,
		true,
		false);

	if (FAILED(res))
	{
		// The created session will be cleaned up by the smart pointer destruction
		DebugConsoleOutput("xboxone_matchmaking_find() - error: couldn't join created session (XblMultiplayerSessionJoin failed with 0x%08x)\n", res);
		return;
	}

	res = XblMultiplayerSessionCurrentUserSetStatus(
		sessionHandle,
		XblMultiplayerSessionMemberStatus::Active);	// a bit redundant, as this could be set in the XblMultiplayerSessionJoin call above, but this is what the XDK ver does

	if (FAILED(res))
	{
		// The created session will be cleaned up by the smart pointer destruction
		DebugConsoleOutput("xboxone_matchmaking_find() - error: couldn't set user status to active (XblMultiplayerSessionCurrentUserSetStatus failed with 0x%08x)\n", res);
		return;
	}

	res = XblMultiplayerSessionCurrentUserSetSecureDeviceAddressBase64(sessionHandle, XSM::GetDeviceAddress()->value);
	if (FAILED(res))
	{
		// The created session will be cleaned up by the smart pointer destruction
		DebugConsoleOutput("xboxone_matchmaking_find() - error: couldn't set secure device address (XblMultiplayerSessionCurrentUserSetSecureDeviceAddressBase64 failed with 0x%08x)\n", res);
		return;
	}

	res = XblMultiplayerSessionSetSessionChangeSubscription(
		sessionHandle,
		XblMultiplayerSessionChangeTypes::Everything);
	if (FAILED(res))
	{
		// The created session will be cleaned up by the smart pointer destruction
		DebugConsoleOutput("xboxone_matchmaking_find() - error: couldn't set session change subscription (XblMultiplayerSessionSetSessionChangeSubscription failed with 0x%08x)\n", res);
		return;
	}			

#ifdef NO_SECURE_CONNECTION
	SetLocalIP(session);
#endif

	// Create and add a new task
	XSMtaskFindSession* findsessiontask = new XSMtaskFindSession();
	findsessiontask->session = session;
	findsessiontask->user_id = user_id;	
	findsessiontask->hopperName = (char*)YYStrDup(hoppername);
	//findsessiontask->sdaTemplateName = sdatemplatename;
	findsessiontask->matchAttributes = (char*)YYStrDup(matchattributes);
	findsessiontask->requestid = XSM::GetNextRequestID();
	findsessiontask->SetState(XSMTS_FindSession_InitialSession);

	XSM::AddTask(findsessiontask);

	// Add session to our tracked list	
	XSM::AddSession(session/*, assoctemplate*/, hoppername, matchattributes);
		
	Result.val = findsessiontask->requestid;
}


namespace XboxOneChat
{
	extern void InitializeSocket();
	extern void Shutdown();
}

void F_XboxOneMatchmakingStart(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_REAL;
	Result.val = -1;

	// Get user
	uint64 user_id = (uint64)YYGetInt64(arg, 0);

	XUM_LOCK_MUTEX
	XUMuser* user = XUM::GetUserFromId(user_id);

	if (user == NULL)
	{
		DebugConsoleOutput("xboxone_matchmaking_start() - error: user not found\n");
		return;
	}

	if (user->IsSignedIn == false)
	{
		DebugConsoleOutput("xboxone_matchmaking_start() - error: specified user not signed in\n");
		return;
	}	
	DebugConsoleOutput("xboxone_matchmaking_start() called\n");
	// Enable multiplayer subscriptions
	user->EnableMultiplayerSubscriptions();

#if !defined(WIN_UAP) && !defined(NO_SECURE_CONNECTION) && YY_CHAT
	if(g_XboxOneGameChatEnabled)
	{
		XboxOneChat::InitializeSocket();
	}
#endif
	Result.val = 0.0;
}

void StopMultiplayerForUser(XUMuser* user)
{
	//if (g_XboxOneGameChatEnabled)
	//{
	//	XboxOneChat::Shutdown();
	//}

	XblContextHandle xbl_context = user->GetXboxLiveContext();

	int* sessionIDs = NULL;
	int numSessionsLeft = 0;

	// Leave any sessions this user owns
	// The back end will boot the user from any other sessions they are in after they time-out
	// Just do this in a brute force fashion (doesn't have to be efficient)
	{
		XSM_LOCK_MUTEX;
		//Windows::Foundation::Collections::IVector<XSMsession^>^ sessions = XSM::GetSessions();
		std::vector<XSMsession*>* sessions = XSM::GetSessions();

		int numsessions = sessions->size();
		sessionIDs = (int*)YYAlloc(sizeof(int) * numsessions);
		for (int i = numsessions - 1; i >= 0; i--) //start at end and iterate backwards (avoids indices changing when sessions are deleted)
		{
			XSMsession* session = sessions->at(i);
			if ((session != NULL) && (session->session != NULL))
			{
				if (session->owner_id == user->XboxUserIdInt)
				{
					//_DestroySessionConnections(session);

					// Leave the session
					HRESULT res = XblMultiplayerSessionLeave(session->session->session_handle);
					if (FAILED(res))
					{
#ifdef XSM_VERBOSE_TRACE
						DebugConsoleOutput("StopMultiplayerForUser() couldn't leave session error 0x%08x\n", res);
#endif
						// Not sure if there's anything else I can do at this point
					}

					// Doing this rather than generating ds_maps here so we don't nest DS_LOCK_MUTEX inside XSM_LOCK_MUTEX
					sessionIDs[numSessionsLeft++] = session->id;					

					// Write session - we don't care about the result at the this point so we don't need to make this a task
					XAsyncBlock* asyncBlock = new XAsyncBlock();
					asyncBlock->queue = XSM::GetTaskQueue();
					asyncBlock->context = NULL;
					asyncBlock->callback = [](XAsyncBlock* asyncBlock)
					{						
						XblMultiplayerWriteSessionResult(asyncBlock, NULL);		// pass in NULL to clean up returned session immediately

						delete asyncBlock;
					};

					res = XblMultiplayerWriteSessionAsync(xbl_context, session->session->session_handle, XblMultiplayerSessionWriteMode::UpdateExisting, asyncBlock);
					if (FAILED(res))
					{
						delete asyncBlock;
					}

					// Remove session from list (and all tasks)
					XSM::DeleteSessionGlobally(session->session);
				}
			}
		}
	}

	if (sessionIDs != NULL)
	{
		for (int i = 0; i < numSessionsLeft; i++)
		{
			// We don't actually need to call DS_LOCK_MUTEX here as it's done inside CreateDsMap and CreateAsyncEventWithDSMap anyway
			int dsMapIndex = CreateDsMap(5, "id", (double)MATCHMAKING_SESSION, (void *)NULL,
				"status", 0.0, "session_left",
				"sessionid", (double)(sessionIDs[i]), NULL,
				"requestid", (double)-1, NULL,
				"error", 0.0, NULL);

			CreateAsyncEventWithDSMap(dsMapIndex, EVENT_OTHER_SOCIAL);
		}

		YYFree(sessionIDs);
	}

	// Disable multiplayer subscriptions
	// I don't *think* this should affect the session writes above, as we're just disabling active monitoring of session changes
	// which shouldn't affect pending callbacks
	user->DisableMultiplayerSubscriptions();
}

void F_XboxOneMatchmakingStop(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_REAL;
	Result.val = -1;
	// Get user	

	// Get user
	uint64 user_id = (uint64)YYGetInt64(arg, 0);

	XUM_LOCK_MUTEX
	XUMuser* user = XUM::GetUserFromId(user_id);

	if (user == NULL)
	{
		DebugConsoleOutput("xboxone_matchmaking_stop() - error: user not found\n");
		return;
	}

	if (user->IsSignedIn == false)
	{
		DebugConsoleOutput("xboxone_matchmaking_stop() - error: specified user not signed in\n");
		return;
	}	
	DebugConsoleOutput("xboxone_matchmaking_stop called\n");
	
	StopMultiplayerForUser(user);

	Result.val = 0.0;
}

void F_XboxOneMatchmakingSessionGetUsers(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	// Unlike PSN this doesn't need to be asynchronous as we have a full copy of the session data already
	// This data should be reasonably up to date as we'll have been listening for session changed events

	Result.kind = VALUE_REAL;
	Result.val = -1;

	int sessionNum = YYGetInt32(arg, 0);
	if (sessionNum < 0.0)
		return;

#ifdef NOV_XDK
	// Try and get the session indicated
	XSMsession^ session = XSM::GetSession((uint32)sessionNum);

	if ((session != nullptr) && (session->session != nullptr))
	{
		DebugConsoleOutput("xboxone_matchmaking_session_get_users called with session id %d\n", sessionNum);
		int dsMapIndex = CreateDsMap(2, "id",(double)MATCHMAKING_SESSION,(void *)NULL,
		"status", 0.0, "session_list_user_results");

		char tmp[256];
		int currresult = 0;
		for(unsigned int i=0; i<session->session->Members->Size; i++) 
		{		
			Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionMember^ member = session->session->Members->GetAt(i);

			if ((member != nullptr) && (member->XboxUserId != nullptr))
			{
				snprintf(tmp, 256, "userId%d", currresult);
				
				uint64 userID = stringtoui64(member->XboxUserId);								
				dsMapAddInt64(dsMapIndex, tmp, userID);

				snprintf(tmp, 256, "userIsOwner%d", currresult);
				if (member->DeviceToken->Equals(session->session->SessionProperties->HostDeviceToken))
				{
					dsMapAddDouble(dsMapIndex, tmp, 1.0);
				}
				else
				{
					dsMapAddDouble(dsMapIndex, tmp, 0.0);
				}				

				currresult++;
			}
		}

		dsMapAddDouble(dsMapIndex, "num_results", currresult);
		dsMapAddDouble(dsMapIndex, "error", 0.0);

		Result.val = dsMapIndex;
	}	
#endif
}

void F_XboxOneMatchmakingSessionLeave(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{	
	Result.kind = VALUE_REAL;
	Result.val = -1;

	int sessionNum = YYGetInt32(arg, 0);
	if (sessionNum < 0)
	{
		DebugConsoleOutput("Attempting to leave a session with a negative session number\n");
		return;
	}

	// Try and get the session indicated
	XSMsession* session = XSM::GetSession((uint32)sessionNum);

	if ((session != NULL) && (session->session != NULL))
	{		
		DebugConsoleOutput("xboxone_matchmaking_session_leave() called with session id %d\n", sessionNum);

		// Fire off an async event (making it async so it matches the PSN version - also allows the same event to be raised in other circumstances)
		int reqID = XSM::GetNextRequestID();
		Result.val = reqID;

		{
			// DS_LOCK_MUTEX;

			int dsMapIndex = CreateDsMap(5, "id", (double)MATCHMAKING_SESSION, (void *)NULL,
				"status", 0.0, "session_left",
				"sessionid", (double)session->id, NULL,
				"requestid", (double)reqID, NULL,
				"error", 0.0, NULL);

			CreateAsyncEventWithDSMap(dsMapIndex, EVENT_OTHER_SOCIAL);
		}

		//_DestroySessionConnections(session);

		const XblMultiplayerSessionMember* currentuser = XblMultiplayerSessionCurrentUser(session->session->session_handle);
		XUMuser* xumuser = XUM::GetUserFromId(currentuser->Xuid);
		XblContextHandle xbl_context = NULL;
		if (xumuser != NULL)
		{
			xbl_context = xumuser->GetXboxLiveContext();
		}
		if (xbl_context == NULL)
		{
#ifdef XSM_VERBOSE_TRACE
			DebugConsoleOutput("xboxone_matchmaking_session_leave() couldn't get xbox live context for user for session\n");
#endif
		}
		else
		{
			HRESULT res = XblMultiplayerSessionLeave(session->session->session_handle);
			if (FAILED(res))
			{
#ifdef XSM_VERBOSE_TRACE
				DebugConsoleOutput("xboxone_matchmaking_session_leave() couldn't leave session error 0x%08x\n", res);
#endif
				// Not sure if there's anything else I can do at this point
			}

			// Write session - we don't care about the result at the this point so we don't need to make this at task
			XAsyncBlock* asyncBlock = new XAsyncBlock();
			asyncBlock->queue = XSM::GetTaskQueue();
			asyncBlock->context = NULL;
			asyncBlock->callback = [](XAsyncBlock* asyncBlock)
			{
				XblMultiplayerWriteSessionResult(asyncBlock, NULL);		// pass in NULL to clean up returned session immediately

				delete asyncBlock;
			};
						
			res = XblMultiplayerWriteSessionAsync(xbl_context, session->session->session_handle, XblMultiplayerSessionWriteMode::UpdateExisting, asyncBlock);
			if (FAILED(res))
			{
				delete asyncBlock;
			}

			PlayFabPartyManager::LeaveNetwork(session->playfabNetworkIdentifier);
		}

		// Remove session from list (and all tasks)
		XSM::DeleteSessionGlobally(session->session);			
	}
	else
	{
		DebugConsoleOutput("xboxone_matchmaking_session_leave: Could not find session to leave\n");
	}
}

void F_XboxOneMatchmakingSendInvites(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{	
	Result.kind = VALUE_REAL;
	Result.val = -1;

	uint64 id = (uint64)(YYGetInt64(arg, 0)); // don't do any rounding on this
	XUMuser* xumuser = XUM::GetUserFromId(id);

	if (xumuser == NULL)
	{
		DebugConsoleOutput("xboxone_matchmaking_send_invites() - error: specified user not found\n");
		return;
	}

	XSMsession* xsmsession = XSM::GetSession(YYGetUint32(arg, 1));
	if (xsmsession == NULL)
	{
		DebugConsoleOutput("xboxone_matchmaking_send_invites() - error: specified session not found\n");
		return;
	}

	if (xsmsession->session == NULL)
	{
		DebugConsoleOutput("xboxone_matchmaking_send_invites() - error: specified session not valid\n");
		return;
	}

	const XblMultiplayerSessionReference* sessref = NULL;
	sessref = XblMultiplayerSessionSessionReference(xsmsession->session->session_handle);
	if (sessref == NULL)
	{
		DebugConsoleOutput("xboxone_matchmaking_send_invites() - error: could not get session reference data for specified session\n");
		return;
	}

	const char* invitemessage = YYGetString(arg, 2);

	Result.val = 0;	

	HRESULT res;
	XAsyncBlock* asyncBlock = new XAsyncBlock();
	asyncBlock->queue = XSM::GetTaskQueue();
	asyncBlock->context = NULL;
	asyncBlock->callback = [](XAsyncBlock* _asyncBlock)
	{
		HRESULT res = XGameUiShowSendGameInviteResult(_asyncBlock);
		/*size_t handlesCount = 1; // must be equal to invites requested
		XblMultiplayerInviteHandle handles[1] = {};
		HRESULT res = XblMultiplayerSendInvitesResult(_asyncBlock, handlesCount, handles);*/

		if (FAILED(res))
		{
			DebugConsoleOutput("xboxone_matchmaking_send_invites() - invitation failed - 0x%08x\n", res);
		}
		else
		{
			DebugConsoleOutput("xboxone_matchmaking_send_invites() - invitation flow completed successfully (this could also mean the user just cancelled the dialog)\n");
		}
		delete _asyncBlock;
	};	

	res = XGameUiShowSendGameInviteAsync(
		asyncBlock,
		xumuser->user,
		sessref->Scid,
		sessref->SessionTemplateName,
		sessref->SessionName,
		NULL,
		invitemessage);
	/*uint64 testtargetid = 2814665023228083;
	res = XblMultiplayerSendInvitesAsync(
		xumuser->GetXboxLiveContext(),
		sessref,
		&testtargetid,
		1,
		0x1233FEA6,
		NULL,
		NULL,
		asyncBlock);*/

	if (FAILED(res))
	{
		DebugConsoleOutput("xboxone_matchmaking_send_invites() - error sending invite (has the local user been signed out?) - 0x%08x\n", res);
		Result.val = -1;
		delete asyncBlock;
	}
}

void F_XboxOneMatchmakingSetJoinableSession(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{	
	Result.kind = VALUE_REAL;
	Result.val = -1;

	// Get user	
	uint64 user_id = (uint64)YYGetInt64(arg, 0);

	XUM_LOCK_MUTEX
	XUMuser* user = XUM::GetUserFromId(user_id);

	if (user == NULL)
	{
		DebugConsoleOutput("xboxone_matchmaking_set_joinable_session() - error: user not found\n");
		return;
	}

	if (user->IsSignedIn == false)
	{
		DebugConsoleOutput("xboxone_matchmaking_set_joinable_session() - error: specified user not signed in\n");
		return;
	}

	//if (YYGetInt32(arg, 0) < 0.0)
		//return;

	// Try and get the session indicated
	XblContextHandle xbl_context = user->GetXboxLiveContext();
	if (!xbl_context)
	{
		DebugConsoleOutput("xboxone_matchmaking_set_joinable_session - exception occurred getting live context\n");

		Result.kind = VALUE_REAL;
		Result.val = -1;
		return;
	}

	int session_id = YYGetInt32(arg, 1);
	XSMsession* session = XSM::GetSession((uint32)session_id);

	DebugConsoleOutput("xboxone_matchmaking_set_joinable_session - called with %d\n", YYGetInt32(arg, 1));

	if (((session != nullptr) && (session->session != nullptr)) || (session_id == -1))
	{		
		XAsyncBlock* asyncBlock = new XAsyncBlock();
		asyncBlock->queue = XSM::GetTaskQueue();
		asyncBlock->context = (void*)session_id;			// put this value into the context pointer so we can tell which function we called
		asyncBlock->callback = [](XAsyncBlock* asyncBlock)
		{
			// The documentation doesn't mention any way of getting the result of this call
			// So we'll just assume it worked :) 
			DebugConsoleOutput("xboxone_matchmaking_set_joinable_session completed\n");

			delete asyncBlock;
		};

		HRESULT res;
		if (session_id == -1)
		{
			res = XblMultiplayerClearActivityAsync(xbl_context, g_XboxSCID, asyncBlock);
		}
		else
		{
			res = XblMultiplayerSetActivityAsync(xbl_context, XblMultiplayerSessionSessionReference(session->session->session_handle), asyncBlock);
		}

		if (FAILED(res))
		{
			delete asyncBlock;
		}
	}
}

int MatchmakingJoinSession(uint64 user_id, const char* handleSessionToJoin, const char* sdatemplatename)
{
	//
	// Retrieve MPSD session from the handle and then attempt to join it
	if (handleSessionToJoin == nullptr)
	{
		DebugConsoleOutput("MatchmakingJoinSession - no handle to join on!");
		return -1;
	}

	// Get user
	XUM_LOCK_MUTEX
	XUMuser* user = XUM::GetUserFromId(user_id);

	if (user == NULL)
	{
		DebugConsoleOutput("MatchmakingJoinSession - error: user not found\n");
		return -1;
	}

	if (user->IsSignedIn == false)
	{
		DebugConsoleOutput("MatchmakingJoinSession - error: specified user not signed in\n");
		return -1;
	}		
	
	XblContextHandle xbl_context = user->GetXboxLiveContext();
	if (!xbl_context)
	{
		DebugConsoleOutput("MatchmakingJoinSession - exception occurred getting live context\n");
		return -1;
	}

	// Generate new session name
	//char sessionName[64];

	//GUID sessionNameGUID;
	//CoCreateGuid(&sessionNameGUID);
	//GUIDtoString(&sessionNameGUID, sessionName);
	//sessionName = Platform::String::Concat(sessionName, L"blah");

#if defined(WIN_UAP) || (defined(XBOX_LIVE_VER) && XBOX_LIVE_VER>=1508) || defined(NO_SECURE_CONNECTION) || (defined(_XDK_VER) && (_XDK_VER >= 0x38390431))
	// Strip curly brackets from session names otherwise the Live stuff generates invalid URIs
	std::wstring strGuid(sessionName->Data());
	if (strGuid.length() > 0 && strGuid.front() == L'{')
	{
		// Remove the {
		strGuid.erase(0, 1);
	}
	if (strGuid.length() > 0 && strGuid.back() == L'}')
	{
		// Remove the }
		strGuid.pop_back();
	}
	sessionName = ref new Platform::String(strGuid.c_str());
#endif

	XblMultiplayerSessionHandle sessionHandle = XblMultiplayerSessionCreateHandle(user_id, NULL, NULL);
	xbl_session_ptr session(new xbl_session(sessionHandle));
	
	HRESULT res = XblMultiplayerSessionJoin(
		sessionHandle,
		NULL,
		false,
		true);

	if (FAILED(res))
	{
		// The created session will be cleaned up by the smart pointer destruction
		DebugConsoleOutput("MatchmakingJoinSession - error: couldn't join created session (XblMultiplayerSessionJoin failed with 0x%08x)\n", res);
		return -1;
	}
	
	res = XblMultiplayerSessionCurrentUserSetSecureDeviceAddressBase64(sessionHandle, XSM::GetDeviceAddress()->value);
	if (FAILED(res))
	{
		// The created session will be cleaned up by the smart pointer destruction
		DebugConsoleOutput("MatchmakingJoinSession - error: couldn't set secure device address (XblMultiplayerSessionCurrentUserSetSecureDeviceAddressBase64 failed with 0x%08x)\n", res);
		return -1;
	}

	res = XblMultiplayerSessionCurrentUserSetSecureDeviceAddressBase64(sessionHandle, XSM::GetDeviceAddress()->value);
	if (FAILED(res))
	{
		// The created session will be cleaned up by the smart pointer destruction
		DebugConsoleOutput("MatchmakingJoinSession - error: couldn't set secure device address (XblMultiplayerSessionCurrentUserSetSecureDeviceAddressBase64 failed with 0x%08x)\n", res);
		return -1;
	}

	res = XblMultiplayerSessionSetSessionChangeSubscription(
		sessionHandle,
		XblMultiplayerSessionChangeTypes::Everything);
	if (FAILED(res))
	{
		// The created session will be cleaned up by the smart pointer destruction
		DebugConsoleOutput("xboxone_matchmaking_create() - error: couldn't set session change subscription (XblMultiplayerSessionSetSessionChangeSubscription failed with 0x%08x)\n", res);
		return -1;
	}

#ifdef NO_SECURE_CONNECTION
	SetLocalIP(session);
#endif

	// Create and add a new task
	XSMtaskJoinSession* joinsessiontask = new XSMtaskJoinSession();
	joinsessiontask->session = session;
	joinsessiontask->user_id = user_id;	
	//joinsessiontask->sdaTemplateName = sdatemplatename;
	joinsessiontask->requestid = XSM::GetNextRequestID();
	joinsessiontask->sessionHandle = YYStrDup(handleSessionToJoin);
	joinsessiontask->SetState(XSMTS_JoinSession_ProcessUpdatedSession);

	XSM::AddTask(joinsessiontask);

	return joinsessiontask->requestid;
}

void F_XboxOneMatchmakingJoinInvite(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_REAL;
	Result.val = -1;

	uint64 id = (uint64)(YYGetInt64(arg, 0)); // don't do any rounding on this
	const char* invite_session_handle = YYGetString(arg, 1);
	const char* sdatemplatename = YYGetString(arg, 3);
	Result.val = MatchmakingJoinSession(id, invite_session_handle, sdatemplatename);
#if defined XBOX_SERVICES
	const char *invite_uri = YYGetString(arg, 1);
	const char *invite_host = YYGetString(arg, 2);

	Windows::Foundation::Uri^ activationUri = ref new Windows::Foundation::Uri(ConvertCharArrayToManagedString(invite_uri));
	Platform::String^ host = ConvertCharArrayToManagedString(invite_host);

	Platform::String^ handleSessionToJoin = nullptr;

      if(AreStringsEqualCaseInsensitive(host, L"inviteHandleAccept"))
      {
          Windows::Foundation::WwwFormUrlDecoder^ decoder =
              activationUri->QueryParsed;
          //if(decoder->Size != 3)
		  unsigned int numparams = decoder->Size;
		  if(decoder->Size < 3)
              return;

          Platform::String^ invitedXuid = decoder->GetAt(0)->Value;
          Platform::String^ handle = decoder->GetAt(1)->Value;
          Platform::String^ senderXuid = decoder->GetAt(2)->Value;

          handleSessionToJoin = handle;
      }
      else if(AreStringsEqualCaseInsensitive(host, L"activityHandleJoin"))
      {
          Windows::Foundation::WwwFormUrlDecoder^ decoder =
              activationUri->QueryParsed;
          if(decoder->Size < 3)
              return;

          Platform::String^ joinerXuid = decoder->GetAt(0)->Value;
          Platform::String^ handle = decoder->GetAt(1)->Value;
          Platform::String^ joineeXuid = decoder->GetAt(2)->Value;

          handleSessionToJoin = handle;
      }

	  uint64 id = (uint64)(YYGetInt64(arg, 0)); // don't do any rounding on this
	  Platform::String^ sdatemplatename = ConvertCharArrayToManagedString(YYGetString(arg, 3));

	  Result.val = MatchmakingJoinSession(id, handleSessionToJoin , sdatemplatename);
#endif	
}

void F_XboxOneMatchmakingJoinSessionHandle(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_REAL;
	Result.val = -1;

	const char *session_handle = YYGetString(arg, 1);	

	uint64 id = (uint64)(YYGetInt64(arg, 0)); // don't do any rounding on this
	const char* sdatemplatename = YYGetString(arg, 2);

	Result.val = MatchmakingJoinSession(id, session_handle, sdatemplatename);
}

void F_XboxOneDebug(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_REAL;
	Result.val = -1;

	const char *dbg_string = YYGetString(arg, 0);
	bool enable = YYGetBool(arg, 1);

	if (strcmp(dbg_string, "gamechat") == 0)
	{
		// GraphicsPerf::ms_DisplayFlags = enable ? DISPLAY_DEBUG_XBGAMECHAT : 0;
	}
}

void YYGameChatDrawDebug( void )
{
/*
#if defined(YYXBOX)
	extern bool g_chatIntegrationLayerInited;
	if (g_chatIntegrationLayerInited)
	{
		GetChatIntegrationLayer()->m_chatManager->ChatSettings->DiagnosticsTraceLevel = Microsoft::Xbox::GameChat::GameChatDiagnosticsTraceLevel::Verbose;

		int packets_in = GetChatIntegrationLayer()->GameUI_GetPacketStatistic(Microsoft::Xbox::GameChat::ChatMessageType::ChatVoiceDataMessage, IncomingPacket);
		int packets_out = GetChatIntegrationLayer()->GameUI_GetPacketStatistic(Microsoft::Xbox::GameChat::ChatMessageType::ChatVoiceDataMessage, OutgoingPacket);

		// draw the buffer number
		GraphicsPerf::oprintf(40, 40, 0xff00000, 0xff0000ff, "gamechat packets in : %d", packets_in);
		GraphicsPerf::oprintf(40, 70, 0xff00000, 0xff0000ff, "gamechat packets out : %d", packets_out);
	}

	extern int g_chat_data_ready;
	GraphicsPerf::oprintf(40, 120, 0xff00000, 0xff0000ff, "gamechat data ready : %d", g_chat_data_ready);
#endif
*/
} // end YYAudioDrawDebug

void F_XboxOneChatAddUserToChannel(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_REAL;
	Result.val = -1;
}

void F_XboxOneChatRemoveUserFromChannel(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_REAL;
	Result.val = -1;
}

void F_XboxOneChatSetMuted(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_REAL;
	Result.val = -1;

	// Get user
	uint64 user_id = 0;
	if (KIND_RValue(&(arg[0])) == VALUE_STRING)
	{
		user_id = XSM::GetSessionUserIDFromGamerTag(YYGetString(arg, 0));
	}
	else
	{
		user_id = (uint64)YYGetInt64(arg, 0);
	}

	if (user_id == 0)
	{
		DebugConsoleOutput("xboxone_chat_set_muted() failed. Invalid user specified");
		return;
	}

	bool shouldMute = YYGetBool(arg, 1);

	PlayFabPartyManager::MuteRemoteUser(0, user_id, shouldMute);


#if !defined(WIN_UAP) && !defined(NO_SECURE_CONNECTION) && YY_CHAT
	if (g_XboxOneGameChatEnabled)
	{
		const char *xuid = YYGetString(arg, 0);
		bool shouldMute = YYGetBool(arg, 1);
		GameChat2IntegrationLayer::Get()->SetUserMute(xuid, shouldMute, true);
	}
	else
	{
		DebugConsoleOutput("xboxone_chat_set_muted failed. GameChat must be enabled in game options.");
	}
#endif
}

void F_XboxOneChatGetMuted(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_BOOL;
	Result.val = 0;

	// Get user
	uint64 user_id = 0;
	if (KIND_RValue(&(arg[0])) == VALUE_STRING)
	{
		user_id = XSM::GetSessionUserIDFromGamerTag(YYGetString(arg, 0));
	}
	else
	{
		user_id = (uint64)YYGetInt64(arg, 0);
}

	if (user_id == 0)
	{
		DebugConsoleOutput("xboxone_chat_get_muted() failed. Invalid user specified");
		return;
	}

	Result.val = PlayFabPartyManager::IsRemoteUserMuted(0, user_id);

#if !defined(WIN_UAP) && !defined(NO_SECURE_CONNECTION) && YY_CHAT
	if (g_XboxOneGameChatEnabled)
	{
		const char* xuid = YYGetString(arg, 0);
		Result.val = (double)GameChat2IntegrationLayer::Get()->GetUserMute(xuid);
	}
	else
	{
		DebugConsoleOutput("xboxone_chat_get_muted failed. GameChat must be enabled in game options.");
	}
#endif
}

void F_XboxOneChatAddUser(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_REAL;
	Result.val = -1;

	// Irrelevant on GDK
}

void F_XboxOneChatRemoveUser(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_REAL;
	Result.val = -1;

	// Irrelevant on GDK
}

uint64 TranslateXDKChatRelationshipToPlayFabPermissions(int _relationship)
{
	uint64 bittranstable[] =
	{
		0x1,						// GameChat2::SendMicrophoneAudio = PlayFabParty::SendMicrophoneAudio
		0x2,						// GameChat2::SendTextToSpeechAudio = PlayFabParty::SendTextToSpeechAudio
		0,							// GameChat2::SendText = doesn't work the same way with PlayFab Party
		0x4 | 0x8,					// GameChat2::ReceiveAudio = PlayFabParty::ReceiveMicrophoneAudio | PlayFabParty::ReceiveTextToSpeechAudio
		0x10,						// GameChat2::ReceiveText = PlayFabParty::ReceiveText
	};

	uint64 permissions = 0;	
	for (int i = 0; i < (sizeof(bittranstable) / sizeof(uint64)); i++)
	{
		if (_relationship & (1 << i))
			permissions |= bittranstable[i];
	}

	return permissions;
}

void F_XboxOneChatSetCommunicationRelationship(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_REAL;
	Result.val = -1;

	// Get users
	uint64 local_user_id = 0;
	if (KIND_RValue(&(arg[0])) == VALUE_STRING)
	{
		local_user_id = XSM::GetSessionUserIDFromGamerTag(YYGetString(arg, 0));
	}
	else
	{
		local_user_id = (uint64)YYGetInt64(arg, 0);
	}

	if (local_user_id == 0)
	{
		DebugConsoleOutput("xboxone_chat_set_communication_relationship() failed. Invalid local user specified");
		return;
	}

	uint64 remote_user_id = 0;
	if (KIND_RValue(&(arg[1])) == VALUE_STRING)
	{
		remote_user_id = XSM::GetSessionUserIDFromGamerTag(YYGetString(arg, 1));
	}
	else
	{
		remote_user_id = (uint64)YYGetInt64(arg, 1);
	}

	if (remote_user_id == 0)
	{
		DebugConsoleOutput("xboxone_chat_set_communication_relationship() failed. Invalid remote user specified");
		return;
	}

	int relationship = YYGetInt32(arg, 2);
	uint64 permissions = TranslateXDKChatRelationshipToPlayFabPermissions(relationship);

	PlayFabPartyManager::SetChatPermissions(local_user_id, remote_user_id, permissions);

#if !defined(WIN_UAP) && !defined(NO_SECURE_CONNECTION) && YY_CHAT
	if (g_XboxOneGameChatEnabled)
	{
		const char* localUser = YYGetString(arg, 0);
		const char* remoteUser = YYGetString(arg, 1);
		int relationship = YYGetInt32(arg, 2);
		GameChat2IntegrationLayer::Get()->SetUserCommunicationRelationship(localUser, remoteUser, relationship);
	}
	else
	{
		DebugConsoleOutput("xboxone_chat_set_communication_relationship failed. GameChat must be enabled in game options.");
	}
#endif
}

void F_XboxOneSetFindSessionTimeout(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_REAL;
	Result.val = 0;

	g_findSessionTimeout = YYGetInt32(arg, 0);
}

void F_PlayFabSendPacket(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_REAL;
	Result.val = -1;	

	// Get user
	uint64 user_id = (uint64)YYGetInt64(arg, 0);

	XUM_LOCK_MUTEX
	XUMuser* user = XUM::GetUserFromId(user_id);

	if (user == NULL)
	{
		DebugConsoleOutput("playfab_send_packet() - error: user not found\n");
		return;
	}

	if (user->IsSignedIn == false)
	{
		DebugConsoleOutput("playfab_send_packet() - error: specified user not signed in\n");
		Result.val = -2;
		return;
	}

	if ((user->playfabState != XUMuserPlayFabState::logged_in) || (user->playfabLocalUser == NULL))
	{
		DebugConsoleOutput("playfab_send_packet() - error: specified user not logged into PlayFab Party network\n");
		Result.val = -3;
		return;
	}

	const char* target = YYGetString(arg, 1);
	int bufferindex = YYGetInt32(arg, 2);
	int size = YYGetInt32(arg, 3);

	const void* pBuff = BufferGetContent(bufferindex);
	int pBuff_size = BufferGetContentSize(bufferindex);

	if (pBuff == NULL || pBuff_size < 0)
	{
		DebugConsoleOutput("playfab_send_packet() - error: specified buffer not found\n");
		Result.val = -5;
		return;
	}

	if (size > pBuff_size)
	{
		DebugConsoleOutput("playfab_send_packet() - error: specified size exceeds buffer size\n");
		Result.val = -6;
		return;
	}

	Result.val = PlayFabPartyManager::SendPacket(user_id, NULL, target, pBuff, size, false);
}

void F_PlayFabSendReliablePacket(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_REAL;
	Result.val = -1;

	// Get user
	uint64 user_id = (uint64)YYGetInt64(arg, 0);

	XUM_LOCK_MUTEX
	XUMuser* user = XUM::GetUserFromId(user_id);

	if (user == NULL)
	{
		DebugConsoleOutput("playfab_send_reliable_packet() - error: user not found\n");
		return;
	}

	if (user->IsSignedIn == false)
	{
		DebugConsoleOutput("playfab_send_reliable_packet() - error: specified user not signed in\n");
		Result.val = -2;
		return;
	}

	if ((user->playfabState != XUMuserPlayFabState::logged_in) || (user->playfabLocalUser == NULL))
	{
		DebugConsoleOutput("playfab_send_reliable_packet() - error: specified user not logged into PlayFab Party network\n");
		Result.val = -3;
		return;
	}

	const char* target = YYGetString(arg, 1);
	int bufferindex = YYGetInt32(arg, 2);
	int size = YYGetInt32(arg, 3);

	const void* pBuff = BufferGetContent(bufferindex);
	int pBuff_size = BufferGetContentSize(bufferindex);

	if (pBuff == NULL || pBuff_size < 0)
	{
		DebugConsoleOutput("playfab_send_reliable_packet() - error: specified buffer not found\n");
		Result.val = -5;
		return;
	}

	if (size > pBuff_size)
	{
		DebugConsoleOutput("playfab_send_reliable_packet() - error: specified size exceeds buffer size\n");
		Result.val = -6;
		return;
	}

	Result.val = PlayFabPartyManager::SendPacket(user_id, NULL, target, pBuff, size, true);
}

void F_PlayFabGetUserAddress(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_REAL;
	Result.val = -1;

	uint64 user_id = (uint64)YYGetInt64(arg, 0);
	const char* entityID = PlayFabPartyManager::GetUserEntityID(user_id);
	if (entityID == NULL)
	{
		YYCreateString(&Result, entityID);
	}	
}
