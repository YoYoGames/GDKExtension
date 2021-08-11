#include "GDKX.h"
#include "SessionManagement.h"
//#include <collection.h> 
//#include "Files/Support/Support_Data_Structures.h"
//#include "Files/Support/Support_Various.h"
#include "SecureConnectionManager.h"
#include <ppltasks.h>

// TODO: hoist some of the common functionality out into functions that we can share between different types of tasks


//using namespace Windows::Foundation;
//#ifndef WIN_UAP
//using namespace Windows::Xbox::System;
//#endif
//using namespace Microsoft::Xbox::Services;
//using namespace Microsoft::Xbox::Services::Multiplayer;
//using namespace Microsoft::Xbox::Services::Matchmaking;
//using namespace Concurrency;

//extern Platform::String^ g_PrimaryServiceConfigId;

extern int g_findSessionTimeout;

//extern void CreateAsynEventWithDSMap(int dsmapindex, int event_index);

//extern uint64 stringtoui64(Platform::String^ _string, char* _pTempBuffToUse = NULL);
//extern "C" void dsMapAddInt64(int _dsMap, char* _key, int64 _value);

bool g_DBG_ishost = true;

void XSMtaskFindSession::SignalFailure()
{	
	int dsMapIndex = CreateDsMap(5, "id",(double)MATCHMAKING_SESSION,(void *)NULL,
	"status", 0.0, "session_find_results",
	//"session_id", (double) respData->roomDataInternal->roomId, NULL,
	"num_results", 0.0, NULL,
	"requestid", (double) requestid, NULL,
	"error", -1.0, NULL);	

	CreateAsyncEventWithDSMap(dsMapIndex,EVENT_OTHER_SOCIAL);

#ifdef XSM_VERBOSE_TRACE
	DebugConsoleOutput("findsession failed: request id %d\n", requestid);
#endif
	
	SetState(XSMTS_Finished);
}

void XSMtaskFindSession::SignalNoSessionsFound()
{	
	int dsMapIndex = CreateDsMap(5, "id",(double)MATCHMAKING_SESSION,(void *)NULL,
	"status", 0.0, "session_find_results",
	//"session_id", (double) respData->roomDataInternal->roomId, NULL,
	"num_results", 0.0, NULL,
	"requestid", (double) requestid, NULL,
	"error", 0.0, NULL);				

	CreateAsyncEventWithDSMap(dsMapIndex,EVENT_OTHER_SOCIAL);

#ifdef XSM_VERBOSE_TRACE
	DebugConsoleOutput("findsession no sessions found: request id %d\n", requestid);
#endif
	
	SetState(XSMTS_Finished);
}

void XSMtaskFindSession::SignalSuccess()
{
	XSMsession* sess = XSM::GetSession(session);

	// Get the Xbox Live ID of the session host (this will be listed as the 'owner' in the event data to maintain vague consistency with the PSN matchmaking stuff)		
	// Note that I'm assuming (though it's not explicitly mentioned) that multiple users on the same machine will have the same device token
	// This means that the user we get here is definitely on the host machine, but is also potentially one of many
	uint64 hostID = 0;

	if ((sess != NULL) && (sess->session != NULL))
	{
		// First get the host device token
		const XblMultiplayerSessionProperties* props = XblMultiplayerSessionSessionProperties(sess->session->session_handle);
		const char* deviceToken = props->HostDeviceToken.Value;
		if (deviceToken[0] != '\0')
		{
			// Now find a user with matching token in the session member list
			const XblMultiplayerSessionMember* memberList = NULL;
			size_t memberCount = 0;
			HRESULT res = XblMultiplayerSessionMembers(sess->session->session_handle, &memberList, &memberCount);
			if (SUCCEEDED(res))
			{				
				for (int i = 0; i < memberCount; i++)
				{
					const XblMultiplayerSessionMember* member = &(memberList[i]);
					if (stricmp(member->DeviceToken.Value, deviceToken) == 0)
					{						
						hostID = member->Xuid;
					}
				}
			}
		}
	}

	int dsMapIndex = CreateDsMap(6, "id",(double)MATCHMAKING_SESSION,(void *)NULL,
	"status", 0.0, "session_find_results",
	//"session_id", (double) respData->roomDataInternal->roomId, NULL,
	"num_results", 1.0, NULL,
	//"sessionOwner0", 0.0, "",
	"sessionId0", sess != nullptr ? sess->id : -1.0, NULL,
	"requestid", (double) requestid, NULL,
	"error", 0.0, NULL);				

	// Add session owner
	DsMapAddInt64(dsMapIndex, "sessionOwner0", hostID);

	CreateAsyncEventWithDSMap(dsMapIndex,EVENT_OTHER_SOCIAL);

#ifdef XSM_VERBOSE_TRACE
	const char* pSessionName = NULL;	
	const XblMultiplayerSessionReference* sessref = NULL;
	if (sess != nullptr)
	{
		sessref = XblMultiplayerSessionSessionReference(sess->session->session_handle);
	}
	if (sessref != NULL)
	{
		pSessionName = YYStrDup(sessref->SessionName);
	}
	else
	{
		pSessionName = YYStrDup("None");		
	}
		
	DebugConsoleOutput("findsession succeeded: request id %d, session id %d, session name %s\n", requestid, sess != nullptr ? sess->id : -1, pSessionName);
	YYFree(pSessionName);
#endif
	
	SetState(XSMTS_Finished);
}

void XSMtaskFindSession::Process()
{
	XSMtaskBase::Process();

	// Some common stuff
	HRESULT res;
	XUMuser* pUser = XUM::GetUserFromId(user_id);
	XblContextHandle xbl_context = NULL;
	if (pUser != NULL)
	{
		xbl_context = pUser->GetXboxLiveContext();
	}

	switch(state)
	{
		case XSMTS_FindSession_InitialSession:
		{
#ifdef XSM_VERBOSE_TRACE
			DebugConsoleOutput("findsession (XSMTS_FindSession_InitialSession): request id %d\n", requestid);
#endif
			// Just kicking off, so try writing our session to the server
			res = WriteSessionAsync(xbl_context, session->session_handle, XblMultiplayerSessionWriteMode::UpdateOrCreateNew,
				[](XAsyncBlock* asyncBlock)
				{
					XSMtaskFindSession* self = (XSMtaskFindSession*)(asyncBlock->context);

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
#ifdef XSM_VERBOSE_TRACE
							DebugConsoleOutput("findsession (XSMTS_FindSession_InitialSession) write succeeded: request id %d\n", self->requestid);
#endif
							self->SetState(XSMTS_FindSession_CreateMatchTicket);

							XSM::OnSessionChanged(self->user_id, session);

							self->waiting = false;
						}
						else
						{
							// Handle failure
#ifdef XSM_VERBOSE_TRACE
							DebugConsoleOutput("findsession (XSMTS_FindSession_InitialSession) write failed: request id %d \n", self->requestid);
#endif
							self->SetState(XSMTS_FindSession_FailureToWrite);
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
				// Fell at the first hurdle
				// Remove the session from our list and set this task to finished
				// Need to create an event to tell the user what happened
				//delete asyncBlock;

#ifdef XSM_VERBOSE_TRACE
				DebugConsoleOutput("findsession (XSMTS_FindSession_InitialSession) write failed: request id %d\n", requestid);
#endif

				SetState(XSMTS_FailureCleanup);
				waiting = false;
			}
		} break;		

		case XSMTS_FindSession_CreateMatchTicket:
		{
#ifdef XSM_VERBOSE_TRACE
			DebugConsoleOutput("findsession (XSMTS_FindSession_CreateMatchTicket): request id %d\n", requestid);
#endif
			
			//TimeSpan ticketWaitTimeout;
			//ticketWaitTimeout.Duration = g_findSessionTimeout * TICKS_PER_SECOND; // 60 sec timeout
			////ticketWaitTimeout.Duration = 5 * TICKS_PER_SECOND; // 60 sec timeout

			XAsyncBlock* asyncBlock = new XAsyncBlock();
			asyncBlock->queue = XSM::GetTaskQueue();
			asyncBlock->context = this;
			asyncBlock->callback = [](XAsyncBlock* asyncBlock)
			{
				XSMtaskFindSession* self = (XSMtaskFindSession*)(asyncBlock->context);

				if (self->state == XSMTS_Finished)
				{
					// This task has been terminated, so do nothing						
				}
				else
				{
					HRESULT res = XblMatchmakingCreateMatchTicketResult(asyncBlock, &(self->ticketResponse));
					if (FAILED(res))
					{
#ifdef XSM_VERBOSE_TRACE
						DebugConsoleOutput("findsession (XSMTS_FindSession_CreateMatchTicket) create match ticket failed: request id %d\n", self->requestid);
#endif
						self->SetState(XSMTS_FailureCleanup);
						self->waiting = false;
					}
					else
					{
#ifdef XSM_VERBOSE_TRACE
						DebugConsoleOutput("findsession (XSMTS_FindSession_CreateMatchTicket) create match ticket succeeded: request id %d ticket id %s\n", self->requestid, self->ticketResponse.matchTicketId);
#endif

						self->SetState(XSMTS_FindSession_WaitMatchTicketResult);
						self->waiting = true;		// we don't want to keep updating this task while waiting for a multiplayer session changed event
					}
				}

				delete asyncBlock;
			};

			res = XblMatchmakingCreateMatchTicketAsync(
				xbl_context,
				*XblMultiplayerSessionSessionReference(session->session_handle),
				g_XboxSCID,
				hopperName,
				g_findSessionTimeout/* * TICKS_PER_SECOND*/, // timeouts are apparently measured in seconds now - see https://forums.xboxlive.com/questions/99245/xblmatchmakingcreatematchticketasync%E3%81%AEtickettimeout.html
				XblPreserveSessionMode::Never,	// don't care about the original session, find\create a new one
				matchAttributes,
				asyncBlock);

			if (FAILED(res))
			{
#ifdef XSM_VERBOSE_TRACE
				DebugConsoleOutput("findsession (XSMTS_FindSession_CreateMatchTicket) create match ticket failed: request id %d\n", requestid);
#endif

				SetState(XSMTS_FailureCleanup);
				waiting = false;

				delete asyncBlock;
			}
			else
			{
				waiting = true;
			}
		} break;

		case XSMTS_FindSession_FoundSession:
		{
#ifdef XSM_VERBOSE_TRACE
			DebugConsoleOutput("findsession (XSMTS_FindSession_FoundSession): request id %d\n", requestid);
#endif
			if (matchedSession == NULL)
			{

#ifdef XSM_VERBOSE_TRACE
				DebugConsoleOutput("findsession (XSMTS_FindSession_FoundSession) matched session invalid: request id %d\n", requestid);
#endif

				// uh-oh
				SetState(XSMTS_FailureCleanup);
			}
			else
			{
				xbl_session_ptr oldsession = session;

				session = matchedSession;
				//SecureDeviceAssociationTemplate^ sdaTemplate = SecureDeviceAssociationTemplate::GetTemplateByName(sdaTemplateName);
				XSM::AddSession(session/*, sdaTemplate*/, hopperName, matchAttributes);				

#ifdef XSM_VERBOSE_TRACE
				DebugConsoleOutput("findsession (XSMTS_FindSession_FoundSession) matched session valid: request id %d\n", requestid);
#endif
				
				// Nuke our old session
				XSM::DeleteSessionGlobally(oldsession);						

				//SetState(XSMTS_FindSession_ProcessUpdatedSession);

				// Connect to playfab network;
				SetState(XSMTS_FindSession_ProcessUpdatedSession);//XSMTS_GetPlayFabNetworkDetailsAndConnect);
			}
		} break;

		case XSMTS_ConnectionToPlayFabNetworkCompleted:
		{
			// This is the exit point from the standard connect-to-playfab-network operation
			// Immediately transition to our next state
			SetState(XSMTS_FindSession_ReportMatchSetup);//XSMTS_FindSession_ProcessUpdatedSession);
		} break;

		case XSMTS_FindSession_ProcessUpdatedSession:
		{
#ifdef XSM_VERBOSE_TRACE
			DebugConsoleOutput("findsession (XSMTS_FindSession_ProcessUpdatedSession): request id %d\n", requestid);
#endif
			res = XblMultiplayerSessionJoin(session->session_handle, NULL, true, false);

			if (FAILED(res))
			{
				// The created session will be cleaned up by the smart pointer destruction
#ifdef XSM_VERBOSE_TRACE
				DebugConsoleOutput("findsession (XSMTS_FindSession_ProcessUpdatedSession): error: couldn't join found session (XblMultiplayerSessionJoin failed with 0x%08x)\n", res);
#endif
				SetState(XSMTS_FailureCleanup);
			}
			else
			{
				res = XblMultiplayerSessionCurrentUserSetStatus(session->session_handle, XblMultiplayerSessionMemberStatus::Active);
				if (FAILED(res))
				{
					// The created session will be cleaned up by the smart pointer destruction
#ifdef XSM_VERBOSE_TRACE
					DebugConsoleOutput("findsession (XSMTS_FindSession_ProcessUpdatedSession): error: couldn't set user status to active (XblMultiplayerSessionCurrentUserSetStatus failed with 0x%08x)\n", res);
#endif
					SetState(XSMTS_FailureCleanup);
				}
				else
				{
					res = XblMultiplayerSessionCurrentUserSetSecureDeviceAddressBase64(session->session_handle, XSM::GetDeviceAddress()->value);
					if (FAILED(res))
					{
						// The created session will be cleaned up by the smart pointer destruction
#ifdef XSM_VERBOSE_TRACE
						DebugConsoleOutput("findsession (XSMTS_FindSession_ProcessUpdatedSession): error: couldn't set secure device address (XblMultiplayerSessionCurrentUserSetSecureDeviceAddressBase64 failed with 0x%08x)\n", res);
#endif
						SetState(XSMTS_FailureCleanup);
					}
					else
					{
						res = XblMultiplayerSessionSetSessionChangeSubscription(session->session_handle, XblMultiplayerSessionChangeTypes::Everything);
						if (FAILED(res))
						{
							// The created session will be cleaned up by the smart pointer destruction
#ifdef XSM_VERBOSE_TRACE
							DebugConsoleOutput("findsession (XSMTS_FindSession_ProcessUpdatedSession): error: couldn't set session change subscription (XblMultiplayerSessionSetSessionChangeSubscription failed with 0x%08x)\n", res);
#endif
							SetState(XSMTS_FailureCleanup);
						}
						else
						{
							// Delete any existing entity ID that's stored in the session (which will exist if this user was in the session previously)							
							if (pUser == NULL)
							{
#ifdef XSM_VERBOSE_TRACE
								DebugConsoleOutput("findsession (XSMTS_FindSession_ProcessUpdatedSession): error: PlayFab local user was NULL\n");
#endif
								res = E_FAIL;

								SetState(XSMTS_FailureCleanup);
							}
							else
							{								
								//char* xuidstring = XuidToString(pUser->XboxUserIdInt);
								//DeleteSessionProperty(xuidstring);									
								//MemoryManager::Free(xuidstring);								
							}
						}
					}
				}
			}
			//session->SetCurrentUserStatus(MultiplayerSessionMemberStatus::Active);
			//session->SetCurrentUserSecureDeviceAddressBase64(SecureDeviceAddress::GetLocal()->GetBase64String());
			//session->SetSessionChangeSubscription(MultiplayerSessionChangeTypes::Everything);

			if (SUCCEEDED(res))
			{
				// Since we've modified this session, we need to write it back to MPSD
				res = WriteSessionAsync(xbl_context, session->session_handle, XblMultiplayerSessionWriteMode::UpdateOrCreateNew,
					[](XAsyncBlock* asyncBlock)
					{
						XSMtaskFindSession* self = (XSMtaskFindSession*)(asyncBlock->context);

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
								xbl_session_ptr session = std::make_shared<xbl_session>(sessionHandle);

								// Okay, we now have to do quality-of-service measurements
								// These will be triggered by session change events
								self->SetState(XSMTS_FindSession_QOS);
								self->waiting = true;

#ifdef XSM_VERBOSE_TRACE
								DebugConsoleOutput("findsession (XSMTS_FindSession_ProcessUpdatedSession) write session succeeded: request id %d\n", self->requestid);
#endif

								XSM::OnSessionChanged(self->user_id, session);
							}
							else
							{
								// Handle failure
#ifdef XSM_VERBOSE_TRACE
								DebugConsoleOutput("findsession (XSMTS_FindSession_ProcessUpdatedSession) write session failed: request id %d\n", self->requestid);
#endif
								self->SetState(XSMTS_FindSession_FailureToWrite);
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
#ifdef XSM_VERBOSE_TRACE
					DebugConsoleOutput("findsession (XSMTS_FindSession_ProcessUpdatedSession) write session failed: request id %d\n", requestid);
#endif
					SetState(XSMTS_FindSession_FailureToWrite);
					waiting = false;
				}

				//SetState(XSMTS_FindSession_WaitMatchTicketResult);
				//waiting = true;		// we don't want to keep updating this task while waiting for a multiplayer session changed event
			}
		} break;

		case XSMTS_FindSession_SetHost:
		{
#ifdef XSM_VERBOSE_TRACE
			DebugConsoleOutput("findsession (XSMTS_FindSession_SetHost): request id %d\n", requestid);
#endif			
			// First check to see if the host has already been set
			const XblMultiplayerSessionProperties* props = XblMultiplayerSessionSessionProperties(session->session_handle);
			if (props == NULL)
			{
#ifdef XSM_VERBOSE_TRACE
				DebugConsoleOutput("findsession (XSMTS_FindSession_SetHost) couldn't get session properties: request id %d\n", requestid);
#endif

				// uh-oh
				SetState(XSMTS_FailureCleanup);
			}
			else
			{
				if ((props->HostDeviceToken.Value[0] != '\0')		// check for non-null device token
					|| (!g_DBG_ishost)
					)
				{
#ifdef XSM_VERBOSE_TRACE
					// Find host in member list
					// The host is always the first member in the list that has the host device token
					const char* hostToken = props->HostDeviceToken.Value;

					const XblMultiplayerSessionMember* memberList = NULL;
					size_t memberCount = 0;

					res = XblMultiplayerSessionMembers(session->session_handle, &memberList, &memberCount);
					if (FAILED(res))
					{
#ifdef XSM_VERBOSE_TRACE
						DebugConsoleOutput("findsession (XSMTS_FindSession_SetHost) couldn't get session member list 0x%08x: request id %d\n", res, requestid);
#endif
					}
					else
					{						
						for (int i = 0; i < memberCount; i++)
						{
							if (stricmp(memberList[i].DeviceToken.Value, hostToken) == 0)
							{
								const XblMultiplayerSessionMember* host = &(memberList[i]);

								// Found the host
								const char* pHostGamerTag = NULL;
								if ((host != nullptr) && (host->Gamertag[0] != '\0'))
								{
									pHostGamerTag = YYStrDup(host->Gamertag);
								}
								else
								{
									pHostGamerTag = YYStrDup("None");									
								}

								DebugConsoleOutput("findsession (XSMTS_FindSession_SetHost) host found: request id %d, gamertag on host %s\n", requestid, pHostGamerTag);
								YYFree(pHostGamerTag);

								break;
							}
						}
					}
#endif

					// Host already set, switch to next state
					if (XSM::IsUserHost(user_id, session))
						SetState(XSMTS_SetupPlayFabNetwork);
					else
						SetState(XSMTS_GetPlayFabNetworkDetailsAndConnect);//XSMTS_FindSession_ReportMatchSetup);
				}
				else
				{
					// See if the current user is on the host candidates list from the session
					const XblMultiplayerSessionMember* userAsMember = NULL;
					const XblDeviceToken* hostDeviceTokens = NULL;
					size_t hostDeviceTokensCount = 0;
					res = XblMultiplayerSessionHostCandidates(session->session_handle, &hostDeviceTokens, &hostDeviceTokensCount);
					if (FAILED(res))
					{
#ifdef XSM_VERBOSE_TRACE
						DebugConsoleOutput("findsession (XSMTS_FindSession_SetHost) couldn't get host candidates list 0x%08x: request id %d\n", res, requestid);
#endif
					}					

					if (hostDeviceTokensCount == 0)
					{
						// Special case instance in which there are no host candidates
						// In this case, just try setting the current user as the host anyway

						const XblMultiplayerSessionMember* memberList = NULL;
						size_t memberCount = 0;

						// apparently memberList will remain valid for the lifetime of the session, so we don't need to worry about it dropping out of scope
						res = XblMultiplayerSessionMembers(session->session_handle, &memberList, &memberCount);	
						if (FAILED(res))
						{
#ifdef XSM_VERBOSE_TRACE
							DebugConsoleOutput("findsession (XSMTS_FindSession_SetHost) couldn't get session member list 0x%08x: request id %d\n", res, requestid);
#endif
						}

						// Find the user in the member list						
						for (int i = 0; i < memberCount; i++)
						{
							const XblMultiplayerSessionMember* member = &(memberList[i]);
							if (member->Xuid == user_id)
							{
								// Check to see if this is the first member in the list which has this device token
								// If it is, we'll try for host ownership. If it isn't we'll just leave it
								bool found = false;
								for (int j = 0; j < i; j++)		// only need to check entries up to the one for the current user
								{
									if (stricmp(memberList[j].DeviceToken.Value, member->DeviceToken.Value) == 0)
									{
										found = true;
									}
								}
								if (!found)
								{
									userAsMember = &(memberList[i]);
								}
								break;
							}
						}
					}
					else
					{
						const XblMultiplayerSessionMember* memberList = NULL;
						size_t memberCount = 0;

						// apparently memberList will remain valid for the lifetime of the session, so we don't need to worry about it dropping out of scope
						res = XblMultiplayerSessionMembers(session->session_handle, &memberList, &memberCount);
						if (FAILED(res))
						{
#ifdef XSM_VERBOSE_TRACE
							DebugConsoleOutput("findsession (XSMTS_FindSession_SetHost) couldn't get session member list 0x%08x: request id %d\n", res, requestid);
#endif
						}

						// Need to get the device token of the current user so we can see if it's in the host candidates list											
						for (int i = 0; i < memberCount; i++)
						{
							const XblMultiplayerSessionMember* member = &(memberList[i]);
							if (member->Xuid == user_id)
							{
								// Check to see if this is the first member in the list which has this device token
								// If it is, we'll try for host ownership. If it isn't we'll just leave it
								bool found = false;
								for (int j = 0; j < i; j++)		// only need to check entries up to the one for the current user
								{
									if (stricmp(memberList[j].DeviceToken.Value, member->DeviceToken.Value) == 0)
									{
										// Technically, we should check to see if this session member also exists in the host candidates list
										// otherwise we could end up in a situation where nobody attempts to set the host token because they're
										// in the host candidates list but aren't the first user with their device token in the session member list
										// I think it seems unlikely though that only a subset of users from a particular machine could be in the host candidates list

										found = true;
									}
								}

								if (!found)
								{
									userAsMember = &(memberList[i]);
								}
								break;
							}
						}

						if (userAsMember != NULL)
						{
							bool found = false;
							for (int i = 0; i < hostDeviceTokensCount; i++)
							{
								if (stricmp(hostDeviceTokens[i].Value, userAsMember->DeviceToken.Value) == 0)								
								{
									found = true;
									break;
								}
							}

							if (!found)
							{
								userAsMember = NULL;
							}
						}
					}

					if (userAsMember != NULL)
					{
						// Try and set us as the host						
						XblMultiplayerSessionSetHostDeviceToken(session->session_handle, userAsMember->DeviceToken);

						// Write session
						res = WriteSessionAsync(xbl_context, session->session_handle, XblMultiplayerSessionWriteMode::SynchronizedUpdate,
							[](XAsyncBlock* asyncBlock)
							{
								XSMtaskFindSession* self = (XSMtaskFindSession*)(asyncBlock->context);

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
										xbl_session_ptr session = std::make_shared<xbl_session>(sessionHandle);

										// Don't change state, we'll re-enter this state again to double check the host has been properly set
										//self->SetState(XSMTS_SetupPlayFabNetwork);//XSMTS_FindSession_ReportMatchSetup);
										self->waiting = false;

#ifdef XSM_VERBOSE_TRACE
										DebugConsoleOutput("findsession (XSMTS_FindSession_SetHost) write session succeeded: request id %d\n", self->requestid);
#endif

										XSM::OnSessionChanged(self->user_id, session);
									}
									else
									{
										// Handle failure
#ifdef XSM_VERBOSE_TRACE
										DebugConsoleOutput("findsession (XSMTS_FindSession_SetHost) write session failed: request id %d\n", self->requestid);
#endif												

										HRESULT res;
										XUMuser* pUser = XUM::GetUserFromId(self->user_id);
										XblContextHandle xbl_context = NULL;
										if (pUser != NULL)
										{
											xbl_context = pUser->GetXboxLiveContext();
										}

										if ((pUser == NULL) || (xbl_context == NULL))
										{
#ifdef XSM_VERBOSE_TRACE
											DebugConsoleOutput("findsession (XSMTS_FindSession_SetHost) user (%d) for session has been lost: request id %d\n", self->user_id, self->requestid);
#endif	
											self->SetState(XSMTS_FindSession_FailureToWrite);
											self->waiting = false;
										}
										else
										{
											// Get current session, then loop around again (should probably add a max retries value)
											// This can happen if multiple users attempt to modify the session at the same time (when using XblMultiplayerSessionWriteMode::SynchronizedUpdate)
											// In this case we want to get the latest version of the session then do the host evaluation again 										
											XAsyncBlock* newasyncBlock = new XAsyncBlock();
											newasyncBlock->queue = XSM::GetTaskQueue();
											newasyncBlock->context = self;
											newasyncBlock->callback = [](XAsyncBlock* asyncBlock)
											{
												XSMtaskFindSession* self = (XSMtaskFindSession*)(asyncBlock->context);

												if (self->state == XSMTS_Finished)
												{
													// This task has been terminated, so do nothing						
												}
												else
												{
													XblMultiplayerSessionHandle sessionHandle;													
													HRESULT res = XblMultiplayerGetSessionResult(asyncBlock, &sessionHandle);
													if (SUCCEEDED(res))
													{
														xbl_session_ptr newsession = std::make_shared<xbl_session>(sessionHandle);
#ifdef XSM_VERBOSE_TRACE
														const XblMultiplayerSessionReference* sessref = XblMultiplayerSessionSessionReference(newsession->session_handle);
														const char* pSessionName = NULL;
														if (sessref != NULL)
														{
															pSessionName = YYStrDup(sessref->SessionName);
														}
														else
														{
															pSessionName = YYStrDup("None");															
														}

														DebugConsoleOutput("findsession (XSMTS_FindSession_SetHost) get session succeeded: request id %d, session name %s\n", self->requestid, pSessionName);
														YYFree(pSessionName);
#endif
														// Replace old session with new one
														XSM::OnSessionChanged(self->user_id, newsession);

														// Don't change state - we'll re-evaluate the host situation
														self->waiting = false;
													}
													else
													{
#ifdef XSM_VERBOSE_TRACE
														DebugConsoleOutput("findsession (XSMTS_FindSession_SetHost) get session failed 0x%08x: request id %d\n", res, self->requestid);
#endif
														self->SetState(XSMTS_FailureCleanup);
														self->waiting = false;
													}
												}

												delete asyncBlock;
											};

											const XblMultiplayerSessionReference* sessref = XblMultiplayerSessionSessionReference(self->session->session_handle);
											res = XblMultiplayerGetSessionAsync(xbl_context, sessref, newasyncBlock);
											if (FAILED(res))
											{
#ifdef XSM_VERBOSE_TRACE
												DebugConsoleOutput("findsession (XSMTS_FindSession_SetHost) get session failed 0x%08x: request id %d\n", res, self->requestid);
#endif
												self->SetState(XSMTS_FailureCleanup);
												self->waiting = false;

												delete newasyncBlock;
											}
											else
											{
												self->waiting = true;	// keep waiting until new async op is finished
											}
										}										
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
#ifdef XSM_VERBOSE_TRACE
							DebugConsoleOutput("findsession (XSMTS_FindSession_SetHost) write session failed: request id %d\n", requestid);
#endif
							SetState(XSMTS_FindSession_FailureToWrite);
							waiting = false;
						}
					}

				}
			}


		} break;

		case XSMTS_FindSession_ReportMatchSetup:
		{
#ifdef XSM_VERBOSE_TRACE
			DebugConsoleOutput("findsession (XSMTS_FindSession_ReportMatchSetup): request id %d\n", requestid);
#endif

			if (reFindingHost == false)
			{
				// Fire off an event to the user, with the match details
				SignalSuccess();
			}

			// Now gather the initial member details
			SetState(XSMTS_FindSession_GetInitialMemberDetails);
			waiting = false;

		} break;

		case XSMTS_FindSession_GetInitialMemberDetails:
		{
#ifdef XSM_VERBOSE_TRACE
			DebugConsoleOutput("findsession (XSMTS_FindSession_GetInitialMemberDetails): request id %d\n", requestid);
#endif

#if 0
			// We can't set up any connections at the moment, so skip this bit
			SetState(XSMTS_FindSession_Completed);
			waiting = false;
#else
			// If we're the host, we want to set up secure device associations to all the other machines in the session
			// If we're one of the clients, we want to wait until we have a secure device association from the host before continuing

			const XblMultiplayerSessionProperties* props = XblMultiplayerSessionSessionProperties(session->session_handle);
			if (props == NULL)
			{
#ifdef XSM_VERBOSE_TRACE
				DebugConsoleOutput("findsession (XSMTS_FindSession_GetInitialMemberDetails) couldn't get session properties: request id %d\n", requestid);
#endif
				SetState(XSMTS_FailureCleanup);
				return;
			}

			const XblMultiplayerSessionMember* memberList = NULL;
			size_t memberCount = 0;
			res = XblMultiplayerSessionMembers(session->session_handle, &memberList, &memberCount);
			if (FAILED(res))
			{
#ifdef XSM_VERBOSE_TRACE
				DebugConsoleOutput("findsession (XSMTS_FindSession_GetInitialMemberDetails) XblMultiplayerSessionMembers() failed with error 0x%08x: request id %d\n", res, requestid);
#endif
				SetState(XSMTS_FailureCleanup);
				return;
			}

			// First, determine if we are the host
			bool IsHost = XSM::IsUserHost(user_id, session);

			if (IsHost)
			{
#ifdef XSM_VERBOSE_TRACE
				DebugConsoleOutput("findsession (XSMTS_FindSession_GetInitialMemberDetails) IsHost=true : request id %d\n", requestid);
#endif				
				// Iterate through the member list for the session
				// Do not return the current user
				// Return other users that share the same device token as the current user (these will be on the same machine) but indicate that they are local
				// This way the game can avoid creating network connections for these users
			
				// Get the device token for this user
				// Since we're the host, we can just read it from the session properties
				const char* localDeviceToken = props->HostDeviceToken.Value;	
				
				for(int i = 0; i < memberCount; i++)
				{			
					const XblMultiplayerSessionMember* member = &(memberList[i]);
					if (stricmp(member->DeviceToken.Value, localDeviceToken) == 0)
					{
						// Don't return this user
						continue;
					}
					else
					{
						// Get entity ID from the member, and signal that they have joined							
						const char* entityID = GetSessionMemberProperty(member, "entityID");
						if (entityID == NULL)
						{
							// This user doesn't have a valid entity string in the session document, so it hasn't been fully set up yet							
							return;
						}

						SignalMemberJoined(member->Xuid, entityID, true);

						YYFree(entityID);						
					}
				}

				SetState(XSMTS_FindSession_Completed);
				waiting = false;
			}
			else
			{
#ifdef XSM_VERBOSE_TRACE
				DebugConsoleOutput("findsession (XSMTS_FindSession_GetInitialMemberDetails) IsHost=false : request id %d\n", requestid);
#endif
				// Wait for a secure device association to be set up with the host
				// At the moment we're enforcing a star topology so all clients can only communicate with the host
				// Need a timeout here in case there's some sort of problem
				// What to do if there is a timeout failure?
				// 1) Check to see if the host is still in the session. If it is, wait a bit longer. Do this up to a maximum number of times (or total time).
				// 2) If the host has left the session, return to the SetHost state? Need to miss out the ReportMatchSetup state so we don't do it multiple times

				// First get the session host, so we can identify when a connection is made

				// First check that there actually is a host				
				if (props->HostDeviceToken.Value[0] == '\0')
				{
#ifdef XSM_VERBOSE_TRACE
					DebugConsoleOutput("findsession (XSMTS_FindSession_GetInitialMemberDetails) No host found in session : request id %d\n", requestid);
#endif
					// Set state back to SetHost
					SetState(XSMTS_FindSession_SetHost);
					reFindingHost = true;
					waiting = false;
				}
				else
				{
#ifdef REPORT_SESSION_JOINED_FOR_ALL_MEMBERS
					uint64 host_id = XSM::GetHost(session);

					if (host_id == 0)
					{
#ifdef XSM_VERBOSE_TRACE
						DebugConsoleOutput("findsession (XSMTS_FindSession_GetInitialMemberDetails) host connection failed : request id %d\n", requestid);
#endif						
						SetState(XSMTS_FailureCleanup);
						waiting = false;
					}					

					for (int i = 0; i < memberCount; i++)
					{
						const XblMultiplayerSessionMember* member = &(memberList[i]);

						if (member->Xuid != user_id)			// don't report local user as joined
						{
							// Get entity ID from the member, and signal that they have joined (if appropriate)							
							const char* entityID = GetSessionMemberProperty(member, "entityID");
							if (entityID == NULL)
							{
								// This user doesn't have a valid entity string in the session document, so it hasn't been fully set up yet			
								if (member->Xuid == host_id)
									return;			// bail out and try again later

								continue;
							}

							SignalMemberJoined(member->Xuid, entityID, (member->Xuid == host_id) ? true : false);

							YYFree(entityID);
						}
					}

#ifdef XSM_VERBOSE_TRACE
					DebugConsoleOutput("findsession (XSMTS_FindSession_GetInitialMemberDetails) received host connection : request id %d\n", requestid);
#endif
					// That's it
					SetState(XSMTS_FindSession_Completed);
					waiting = false;
#else

					// Get the host device token
					const char* hostToken = props->HostDeviceToken.Value;
					const XblMultiplayerSessionMember* host = NULL;

					// Iterate through the member list and find the session member with this token
					// UPDATE: what happens if we have multiple users on the same host machine? This currently only reports the first, but this may not be the *actual* host
					// Does this matter though? We'll still have an SDA set up with that machine. The problem comes in terms of how we detect and report that the host has disconnected
					// and how we determine the new host. For host disconnection reporting, currently we 
					// Could we enforce a rule that only one user on each machine attempts to become the host? And that user has to be the one that is listed first in the member list?					
					for (int i = 0; i < memberCount; i++)
					{
						const XblMultiplayerSessionMember* member = &(memberList[i]);
						if (stricmp(member->DeviceToken.Value, hostToken) == 0)
						{
							host = member;
							break;
						}
#ifndef DISABLE_GAME_CHAT
						if (!session->Members->GetAt(i)->XboxUserId->Equals(user->XboxUserId))
						{
							extern void CreateChatConnectionForSessionMember(MultiplayerSessionMember ^_member);
							CreateChatConnectionForSessionMember(session->Members->GetAt(i));
						}
#endif
					}

					if (host != NULL)
					{
						// Get entity ID from the member, and signal that they have joined	
						const XblMultiplayerSessionMember* member = host;
						const char* entityID = GetSessionMemberProperty(member, "entityID");
						if (entityID == NULL)
						{
							// This user doesn't have a valid entity string in the session document, so it hasn't been fully set up yet							
							return;
						}

						SignalMemberJoined(member->Xuid, entityID, true);

						MemoryManager::Free(entityID);						

#ifdef XSM_VERBOSE_TRACE
						dbg_csol.Output("findsession (XSMTS_FindSession_GetInitialMemberDetails) received host connection : request id %d\n", requestid);
#endif
						// That's it
						SetState(XSMTS_FindSession_Completed);
						waiting = false;
					}
					else
					{
#ifdef XSM_VERBOSE_TRACE
						dbg_csol.Output("findsession (XSMTS_FindSession_GetInitialMemberDetails) host connection failed : request id %d\n", requestid);
#endif						
						SetState(XSMTS_FailureCleanup);
						waiting = false;
					}
#endif
					
				}

			}
#endif
		} break;

		case XSMTS_FindSession_Completed:
		{
#ifdef XSM_VERBOSE_TRACE
			DebugConsoleOutput("findsession (XSMTS_FindSession_Completed): request id %d\n", requestid);
#endif

			bool IsHost = XSM::IsUserHost(user_id, session);
			if (IsHost)
			{
				// Right, if we're the host crank up a session advertisement operation
				XSMtaskAdvertiseSession* adtask = new XSMtaskAdvertiseSession();
				adtask->session = session;
				adtask->user_id = user_id;				
				adtask->hopperName = (char*)YYStrDup(hopperName);
				adtask->matchAttributes = (char*)YYStrDup(matchAttributes);
				adtask->requestid = requestid;			// just inherit the request ID of this context				
				adtask->SetState(XSMTS_AdvertiseSession_CreateMatchTicket);

				XSM::AddTask(adtask);
			}

			SetState(XSMTS_Finished);
		} break;

		case XSMTS_FindSession_NoSessionsFound:
		{			
#ifdef XSM_VERBOSE_TRACE
			DebugConsoleOutput("findsession (XSMTS_FindSession_NoSessionsFound): request id %d\n", requestid);
#endif
			// Leave the session
			res = XblMultiplayerSessionLeave(session->session_handle);
			if (FAILED(res))
			{
#ifdef XSM_VERBOSE_TRACE
				DebugConsoleOutput("findsession (XSMTS_FindSession_NoSessionsFound) couldn't leave session: request id %d\n", requestid);
#endif
				// Not sure if there's anything else I can do at this point
			}

			// Write session
			res = WriteSessionAsync(xbl_context, session->session_handle, XblMultiplayerSessionWriteMode::UpdateExisting,
				[](XAsyncBlock* asyncBlock)
				{
					XSMtaskFindSession* self = (XSMtaskFindSession*)(asyncBlock->context);

					if (self->state == XSMTS_Finished)
					{
						// This task has been terminated, so do nothing		
						DebugConsoleOutput("findsession (XSMTS_FindSession_NoSessionsFound) already in finished state\n");
					}
					else
					{
						// Regardless of what happened, signal destroyed and bail
						XblMultiplayerWriteSessionResult(asyncBlock, NULL);		// pass in NULL to clean up returned session immediately

						// Regardless of what happened, signal failure and bail
						self->SignalNoSessionsFound();

						XSM::DeleteSessionGlobally(self->session);

#ifdef XSM_VERBOSE_TRACE
						DebugConsoleOutput("findsession (XSMTS_FindSession_NoSessionsFound) write succeeded: request id %d\n", self->requestid);
#endif
					}

					delete asyncBlock;
				});

			if (FAILED(res))
			{
				SignalNoSessionsFound();

				XSM::DeleteSessionGlobally(session);

#ifdef XSM_VERBOSE_TRACE
				DebugConsoleOutput("findsession (XSMTS_FindSession_NoSessionsFound) write failed: request id %d\n", requestid);
#endif
			}
			else
			{
				waiting = true;
			}
		} break;

		case XSMTS_FailureCleanup:
		{
#ifdef XSM_VERBOSE_TRACE
			DebugConsoleOutput("findsession (XSMTS_FailureCleanup): request id %d\n", requestid);
#endif
			// Leave the session
			res = XblMultiplayerSessionLeave(session->session_handle);
			if (FAILED(res))
			{
#ifdef XSM_VERBOSE_TRACE
				DebugConsoleOutput("findsession (XSMTS_FailureCleanup) exception (0x%08x): request id %d\n", res, requestid);
#endif
				// Not sure if there's anything else I can do at this point
			}

			// Write session
			res = WriteSessionAsync(xbl_context, session->session_handle, XblMultiplayerSessionWriteMode::UpdateExisting,
				[](XAsyncBlock* asyncBlock)
				{
					XSMtaskFindSession* self = (XSMtaskFindSession*)(asyncBlock->context);

					if (self->state == XSMTS_Finished)
					{
						// This task has been terminated, so do nothing		
						DebugConsoleOutput("findsession (XSMTS_FailureCleanup) already in finished state\n");
					}
					else
					{
						// Regardless of what happened, signal destroyed and bail						
						XblMultiplayerWriteSessionResult(asyncBlock, NULL);		// pass in NULL to clean up returned session immediately

						// Regardless of what happened, signal failure and bail
						self->SignalFailure();

						XSM::DeleteSessionGlobally(self->session);

#ifdef XSM_VERBOSE_TRACE
						DebugConsoleOutput("findsession (XSMTS_FailureCleanup) write succeeded: request id %d\n", self->requestid);
#endif
					}

					delete asyncBlock;
				});

			if (FAILED(res))
			{
				SignalFailure();

				XSM::DeleteSessionGlobally(session);

#ifdef XSM_VERBOSE_TRACE
				DebugConsoleOutput("findsession (XSMTS_FailureCleanup) write failed: request id %d\n", requestid);
#endif
			}
			else
			{
				waiting = true;
			}

		} break;

		case XSMTS_FindSession_FailureToWrite:
		{
#ifdef XSM_VERBOSE_TRACE
			DebugConsoleOutput("findsession (XSMTS_FindSession_FailureToWrite): request id %d\n", requestid);
#endif
			SignalDestroyed();

			XSM::DeleteSessionGlobally(session);			

			//XSMsession^ xsmsession = XSM::GetSession(session);
			//XSM::DeleteSession(xsmsession->id);			
		} break;
	}
}

void XSMtaskFindSession::ProcessSessionChanged(xbl_session_ptr _updatedsession)
{	
	// Some common stuff
	HRESULT res;
	XUMuser* pUser = XUM::GetUserFromId(user_id);
	XblContextHandle xbl_context = NULL;
	if (pUser != NULL)
	{
		xbl_context = pUser->GetXboxLiveContext();
	}

	// Note to self: Probably best to invert this logic so we check the changes first then test which state we are in
	// (A little worried about missing changes that occur if we are not in the correct state to receive them)
	switch(state)
	{
		case XSMTS_FindSession_WaitMatchTicketResult:
		{								
#ifdef XSM_VERBOSE_TRACE
			DebugConsoleOutput("findsession (XSMTS_FindSession_WaitMatchTicketResult): request id %d\n", requestid);
#endif
			// Compare the old and new sessions
			XblMultiplayerSessionChangeTypes changes = XblMultiplayerSessionCompare(_updatedsession->session_handle, session->session_handle);			

			if ((changes & XblMultiplayerSessionChangeTypes::MatchmakingStatusChange) == XblMultiplayerSessionChangeTypes::MatchmakingStatusChange)
			{

#ifdef XSM_VERBOSE_TRACE
				DebugConsoleOutput("findsession (XSMTS_FindSession_WaitMatchTicketResult) matchmaking status change: request id %d\n", requestid);
#endif
				const XblMultiplayerMatchmakingServer* mmstatus = XblMultiplayerSessionMatchmakingServer(_updatedsession->session_handle);
				if (mmstatus == NULL)
				{
#ifdef XSM_VERBOSE_TRACE
					DebugConsoleOutput("findsession (XSMTS_FindSession_WaitMatchTicketResult) couldn't get matchmaking server data: request id %d\n", requestid);
#endif
					SetState(XSMTS_FailureCleanup);
					waiting = false;
				}
				else
				{
					if ((mmstatus->Status == XblMatchmakingStatus::Expired) ||
						(mmstatus->Status == XblMatchmakingStatus::Canceled))
					{
#ifdef XSM_VERBOSE_TRACE						
						DebugConsoleOutput("expired configid %s, hopperName %s, ticketid %s\n", g_XboxSCID, hopperName, ticketResponse.matchTicketId);
#endif

						// Delete the match ticket (don't wait for this to complete)
						DeleteMatchTicketAsync(xbl_context, g_XboxSCID, hopperName, ticketResponse.matchTicketId);						

						SetState(XSMTS_FindSession_NoSessionsFound);
						waiting = false;
					}
					else if (mmstatus->Status == XblMatchmakingStatus::Found)
					{
	#ifdef XSM_VERBOSE_TRACE
						DebugConsoleOutput("found\n");
	#endif
						// Delete the match ticket (don't wait for this to complete)
						DeleteMatchTicketAsync(xbl_context, g_XboxSCID, hopperName, ticketResponse.matchTicketId);						

						// Found a new session

						// Remove old session
						// How do we handle cases where other operations are referencing the same session? Ref count?
						// What happens if an event occurs referencing the old session?

						//MultiplayerSessionReference^ sessionRef = _updatedsession->MatchmakingServer->TargetSessionRef;
						//MultiplayerSession^ resultSession = ref new MultiplayerSession(user->GetXboxLiveContext(), sessionRef);
						//WriteSessionSync( xboxLiveContext, resultSession, MultiplayerSessionWriteMode::UpdateOrCreateNew );
						//matchmakingComplete = true;		

						XAsyncBlock* asyncBlock = new XAsyncBlock();
						asyncBlock->queue = XSM::GetTaskQueue();
						asyncBlock->context = this;
						asyncBlock->callback = [](XAsyncBlock* asyncBlock)
						{
							XSMtaskFindSession* self = (XSMtaskFindSession*)(asyncBlock->context);
							if (self->state == XSMTS_Finished)
							{
								// This task has been terminated, so do nothing						
							}
							else
							{
								XblMultiplayerSessionHandle sessionHandle;								
								HRESULT res = XblMultiplayerGetSessionResult(asyncBlock, &sessionHandle);
								if (SUCCEEDED(res))
								{
									xbl_session_ptr newsession = std::make_shared<xbl_session>(sessionHandle);
#ifdef XSM_VERBOSE_TRACE
									const char* pSessionName = NULL;
									const XblMultiplayerSessionReference* sessref = NULL;
									if (newsession != NULL)
									{
										sessref = XblMultiplayerSessionSessionReference(newsession->session_handle);
									}
									if (sessref != NULL)
									{
										pSessionName = YYStrDup(sessref->SessionName);
									}
									else
									{
										pSessionName = YYStrDup("None");										
									}

									DebugConsoleOutput("findsession (XSMTS_FindSession_WaitMatchTicketResult) found new session: request id %d, session name %s\n", self->requestid, pSessionName);
									YYFree(pSessionName);
#endif

									self->SetState(XSMTS_FindSession_FoundSession);
									self->waiting = false;

									// Remember, the session hasn't changed - it's a new session
									self->matchedSession = newsession;
								}
								else
								{
#ifdef XSM_VERBOSE_TRACE
									DebugConsoleOutput("findsession (XSMTS_FindSession_WaitMatchTicketResult) get session failed: request id %d\n", self->requestid);
#endif
									self->SetState(XSMTS_FailureCleanup);
									self->waiting = false;
								}
							}

							delete asyncBlock;
						};

						res = XblMultiplayerGetSessionAsync(xbl_context, &(mmstatus->TargetSessionRef), asyncBlock);
						if (FAILED(res))
						{
#ifdef XSM_VERBOSE_TRACE
							DebugConsoleOutput("advertisesession (XSMTS_AdvertiseSession_WaitMatchTicketResult) XblMultiplayerGetSessionAsync failed 0x%08x: request id %d\n", res, requestid);
#endif
							SetState(XSMTS_FailureCleanup);
							waiting = false;

							delete asyncBlock;
						}
						else
						{
							waiting = true;
						}						
					}
					else if (mmstatus->Status == XblMatchmakingStatus::Searching)
					{
						// Do nothing at the moment
	#ifdef XSM_VERBOSE_TRACE
						DebugConsoleOutput("searching\n");
	#endif
					}
					else
					{
	#ifdef XSM_VERBOSE_TRACE
						if (mmstatus->Status == XblMatchmakingStatus::None)
							DebugConsoleOutput("none\n");
						else if (mmstatus->Status == XblMatchmakingStatus::Unknown)
							DebugConsoleOutput("unknown\n");
	#endif

						// Delete the match ticket (don't wait for this to complete)
						DeleteMatchTicketAsync(xbl_context, g_XboxSCID, hopperName, ticketResponse.matchTicketId);						

						// Try another match ticket
						SetState(XSMTS_FindSession_CreateMatchTicket);
						waiting = false;
					}
				}
			}									
		} break;		

		case XSMTS_FindSession_QOS:
		{
#ifdef XSM_VERBOSE_TRACE
			DebugConsoleOutput("findsession (XSMTS_FindSession_QOS): request id %d\n", requestid);
#endif
			// Compare the old and new sessions
			XblMultiplayerSessionChangeTypes changes = XblMultiplayerSessionCompare(_updatedsession->session_handle, session->session_handle);			

			if ((changes & XblMultiplayerSessionChangeTypes::InitializationStateChange) == XblMultiplayerSessionChangeTypes::InitializationStateChange)
			{
#ifdef XSM_VERBOSE_TRACE
				DebugConsoleOutput("findsession (XSMTS_FindSession_QOS) initialisation state change: request id %d\n", requestid);
#endif
				const XblMultiplayerSessionInitializationInfo* initInfo = XblMultiplayerSessionGetInitializationInfo(_updatedsession->session_handle);
				if (initInfo == NULL)
				{
#ifdef XSM_VERBOSE_TRACE
					DebugConsoleOutput("findsession (XSMTS_FindSession_QOS) couldn't get initialisation info: request id %d\n", requestid);
#endif
					SetState(XSMTS_FailureCleanup);
					waiting = false;
				}
				else
				{
					if (initInfo->Episode == 0)
					{
						// QOS phase complete
						SetState(XSMTS_FindSession_SetHost);
						waiting = false;

#ifdef XSM_VERBOSE_TRACE
						DebugConsoleOutput("QOS completed\n", requestid);
#endif
					}
					else
					{
						XblMultiplayerInitializationStage stage = initInfo->Stage;

						// I think we might only need to consider the 'measuring' state if we're relying on the MPSD service to do the QOS evaluation
						switch (stage)
						{
						case XblMultiplayerInitializationStage::None:
						{
#ifdef XSM_VERBOSE_TRACE
							DebugConsoleOutput("stage None\n", requestid);
#endif
						} break;

						case XblMultiplayerInitializationStage::Unknown:
						{
#ifdef XSM_VERBOSE_TRACE
							DebugConsoleOutput("stage Unknown\n", requestid);
#endif
						} break;

						case XblMultiplayerInitializationStage::Joining:
						{
#ifdef XSM_VERBOSE_TRACE
							DebugConsoleOutput("stage Joining\n", requestid);
#endif
						} break;

						case XblMultiplayerInitializationStage::Measuring:
						{
#ifdef XSM_VERBOSE_TRACE
							DebugConsoleOutput("stage Measuring\n", requestid);
#endif
						} break;

						case XblMultiplayerInitializationStage::Evaluating:
						{
#ifdef XSM_VERBOSE_TRACE
							DebugConsoleOutput("stage Evaluating\n", requestid);
#endif
						} break;

						case XblMultiplayerInitializationStage::Failed:
						{
#ifdef XSM_VERBOSE_TRACE
							DebugConsoleOutput("stage Failed\n", requestid);
#endif
						} break;
						}
					}
				}
			}
			else
			{
#ifdef XSM_VERBOSE_TRACE
				DebugConsoleOutput("findsession (XSMTS_FindSession_QOS ) No Initialisation state change, forcing complete on QOS query\n", requestid);
#endif
				SetState(XSMTS_FindSession_SetHost);
				waiting = false;

#ifdef XSM_VERBOSE_TRACE
				DebugConsoleOutput("QOS completed\n", requestid);
#endif
			}
			
		} break;

		default:
		{
			
		} break;
	}

	// Update session
	session = _updatedsession;
}

void XSMtaskFindSession::ProcessSessionDeleted(xbl_session_ptr _sessiontodelete)
{
	// Not sure of the best way of terminating in-flight async ops
	// Probably the best way is to check to see if 'waiting' is true then set the state to Finished - we can then check for this inside the async op lambda
	if (waiting)
	{
		SetState(XSMTS_Finished);
	}
	else
	{
		// Do something else depending on state

		// Currently, just set to finished too
		SetState(XSMTS_Finished);
	}
}
