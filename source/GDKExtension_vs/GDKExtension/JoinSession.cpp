//
// Copyright (C) 2020 Opera Norway AS. All rights reserved.
//
// This file is an original work developed by Opera.
//

#include "GDKX.h"
#include "SessionManagement.h"
#include "SecureConnectionManager.h"
#include <ppltasks.h>

// TODO: hoist some of the common functionality out into functions that we can share between different types of tasks

bool g_DBG_ishost_2 = true;

void XSMtaskJoinSession::SignalFailure()
{	
	int dsMapIndex = CreateDsMap(5, "id",(double)MATCHMAKING_SESSION,(void *)NULL,
	"status", 0.0, "session_find_results",
	//"session_id", (double) respData->roomDataInternal->roomId, NULL,
	"num_results", 0.0, NULL,
	"requestid", (double) requestid, NULL,
	"error", -1.0, NULL);	

	CreateAsyncEventWithDSMap(dsMapIndex,EVENT_OTHER_SOCIAL);

	XSM_VERBOSE_OUTPUT("joinsession failed: request id %d\n", requestid);
	
	SetState(XSMTS_Finished);
}

void XSMtaskJoinSession::SignalNoSessionsFound()
{	
	int dsMapIndex = CreateDsMap(5, "id",(double)MATCHMAKING_SESSION,(void *)NULL,
	"status", 0.0, "session_find_results",
	//"session_id", (double) respData->roomDataInternal->roomId, NULL,
	"num_results", 0.0, NULL,
	"requestid", (double) requestid, NULL,
	"error", 0.0, NULL);				

	CreateAsyncEventWithDSMap(dsMapIndex,EVENT_OTHER_SOCIAL);

	XSM_VERBOSE_OUTPUT("joinsession no sessions found: request id %d\n", requestid);
	
	SetState(XSMTS_Finished);
}

void XSMtaskJoinSession::SignalSuccess()
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
		
	DebugConsoleOutput("joinsession succeeded: request id %d, session id %d, session name %s\n", requestid, sess != nullptr ? sess->id : -1, pSessionName);
	YYFree(pSessionName);
#endif
	
	SetState(XSMTS_Finished);
}

const char *names[] =
{
	"XSMTS_JoinSession_InitialSession",	
	"XSMTS_JoinSession_WaitMatchTicketResult",
	"XSMTS_JoinSession_FoundSession",
	"XSMTS_JoinSession_ProcessUpdatedSession",
	"XSMTS_JoinSession_QOS",
	"XSMTS_JoinSession_SetHost",
	"XSMTS_JoinSession_ReportMatchSetup",
	"XSMTS_JoinSession_GetInitialMemberDetails",
	"XSMTS_JoinSession_Completed",
	"XSMTS_JoinSession_NoSessionsFound",	
	"XSMTS_JoinSession_FailureToWrite"
};

void XSMtaskJoinSession::Process()
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
	if (state > 0)
	{
		DebugConsoleOutput("Join in state %s\n", names[state - 1]);
	}

	switch(state)
	{
		case XSMTS_JoinSession_FoundSession:
		{
			XSM_VERBOSE_OUTPUT("joinsession (XSMTS_JoinSession_FoundSession): request id %d\n", requestid);
			if (joinSession == NULL)
			{

				XSM_VERBOSE_OUTPUT("joinsession (XSMTS_JoinSession_FoundSession) matched session invalid: request id %d\n", requestid);

				// uh-oh
				SetState(XSMTS_FailureCleanup);
			}
			else
			{
				xbl_session_ptr oldsession = session;

				session = joinSession;
				//SecureDeviceAssociationTemplate^ sdaTemplate = SecureDeviceAssociationTemplate::GetTemplateByName(sdaTemplateName);
				XSM::AddSession(session/*, sdaTemplate*/, NULL, NULL);;

				XSM_VERBOSE_OUTPUT("joinsession (XSMTS_JoinSession_FoundSession) matched session valid: request id %d\n", requestid);
				
				// Nuke our old session
				XSM::DeleteSessionGlobally(oldsession);																		

				SetState(XSMTS_JoinSession_ProcessUpdatedSession);
			}
		} break;

		case XSMTS_ConnectionToPlayFabNetworkCompleted:
		{
			// This is the exit point from the standard connect-to-playfab-network operation
			// Immediately transition to our next state
			SetState(XSMTS_JoinSession_ReportMatchSetup);//XSMTS_FindSession_ProcessUpdatedSession);
		} break;

		case XSMTS_JoinSession_ProcessUpdatedSession:
		{
			XSM_VERBOSE_OUTPUT("joinsession (XSMTS_JoinSession_ProcessUpdatedSession): request id %d\n", requestid);
			res = XblMultiplayerSessionCurrentUserSetStatus(session->session_handle, XblMultiplayerSessionMemberStatus::Active);
			if (FAILED(res))
			{
				// The created session will be cleaned up by the smart pointer destruction
				DebugConsoleOutput("joinsession (XSMTS_JoinSession_ProcessUpdatedSession): error: couldn't set user status to active (XblMultiplayerSessionCurrentUserSetStatus failed with 0x%08x)\n", res);
				SetState(XSMTS_FailureCleanup);
			}
			else
			{
				res = XblMultiplayerSessionCurrentUserSetSecureDeviceAddressBase64(session->session_handle, XSM::GetDeviceAddress()->value);
				if (FAILED(res))
				{
					// The created session will be cleaned up by the smart pointer destruction
					DebugConsoleOutput("joinsession (XSMTS_JoinSession_ProcessUpdatedSession): error: couldn't set secure device address (XblMultiplayerSessionCurrentUserSetSecureDeviceAddressBase64 failed with 0x%08x)\n", res);
					SetState(XSMTS_FailureCleanup);
				}
				else
				{
					res = XblMultiplayerSessionSetSessionChangeSubscription(session->session_handle, XblMultiplayerSessionChangeTypes::Everything);
					if (FAILED(res))
					{
						// The created session will be cleaned up by the smart pointer destruction
						DebugConsoleOutput("joinsession (XSMTS_JoinSession_ProcessUpdatedSession): error: couldn't set session change subscription (XblMultiplayerSessionSetSessionChangeSubscription failed with 0x%08x)\n", res);
						SetState(XSMTS_FailureCleanup);
					}
					else
					{
						// Delete any existing entity ID that's stored in the session (which will exist if this user was in the session previously)							
						if (pUser == NULL)
						{
							XSM_VERBOSE_OUTPUT("findsession (XSMTS_FindSession_ProcessUpdatedSession): error: PlayFab local user was NULL\n");
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

			if (SUCCEEDED(res))
			{
				// Since we've modified this session, we need to write it back to MPSD
				res = WriteSessionAsync(xbl_context, session->session_handle, XblMultiplayerSessionWriteMode::UpdateExisting,
					[](XAsyncBlock* asyncBlock)
					{
						XSMtaskJoinSession* self = (XSMtaskJoinSession*)(asyncBlock->context);

						if (self->state == XSMTS_Finished)
						{
							// This task has been terminated, so do nothing						
						}
						else
						{
							XblMultiplayerSessionHandle sessionHandle;							
							HRESULT hr = XblMultiplayerWriteSessionByHandleResult(asyncBlock, &sessionHandle);
							if (SUCCEEDED(hr))
							{
								xbl_session_ptr session = std::make_shared<xbl_session>(sessionHandle);

								self->SetState(XSMTS_JoinSession_SetHost);
								self->waiting = false;

								self->session = session;
								XSM::AddSession(session/*, sdaTemplate*/, NULL, NULL);

								XSM_VERBOSE_OUTPUT("joinsession (XSMTS_JoinSession_ProcessUpdatedSession) write session succeeded: request id %d\n", self->requestid);

								XSM::OnSessionChanged(self->user_id, session);
							}
							else
							{
								// Handle failure
								XSM_VERBOSE_OUTPUT("joinsession (XSMTS_JoinSession_ProcessUpdatedSession) write session failed: request id %d\n", self->requestid);
								self->SetState(XSMTS_JoinSession_FailureToWrite);
								self->waiting = false;
							}
						}

						delete asyncBlock;
					},
					sessionHandle);		// we're wanting to write to this specific session

				if (FAILED(res))
				{
					XSM_VERBOSE_OUTPUT("joinsession (XSMTS_JoinSession_ProcessUpdatedSession) write session failed: request id %d\n", requestid);
					SetState(XSMTS_JoinSession_FailureToWrite);
					waiting = false;
				}
				else
				{
					waiting = true;
				}

				//SetState(XSMTS_JoinSession_WaitMatchTicketResult);
				//waiting = true;		// we don't want to keep updating this task while waiting for a multiplayer session changed event
			}
		} break;

		case XSMTS_JoinSession_SetHost:
		{
			XSM_VERBOSE_OUTPUT("joinsession (XSMTS_JoinSession_SetHost): request id %d\n", requestid);
			// First check to see if the host has already been set
			const XblMultiplayerSessionProperties* props = XblMultiplayerSessionSessionProperties(session->session_handle);
			if (props == NULL)
			{
				XSM_VERBOSE_OUTPUT("joinsession (XSMTS_JoinSession_SetHost) couldn't get session properties: request id %d\n", requestid);

				// uh-oh
				SetState(XSMTS_FailureCleanup);
			}
			else
			{
				if ((props->HostDeviceToken.Value[0] != '\0')		// check for non-null device token
					|| (!g_DBG_ishost_2)
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
						XSM_VERBOSE_OUTPUT("joinsession (XSMTS_JoinSession_SetHost) couldn't get session member list 0x%08x: request id %d\n", res, requestid);
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

								DebugConsoleOutput("joinsession (XSMTS_JoinSession_SetHost) host found: request id %d, gamertag on host %s\n", requestid, pHostGamerTag);
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
						SetState(XSMTS_GetPlayFabNetworkDetailsAndConnect);//SetState(XSMTS_JoinSession_ReportMatchSetup);
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
						XSM_VERBOSE_OUTPUT("joinsession (XSMTS_JoinSession_SetHost) couldn't get host candidates list 0x%08x: request id %d\n", res, requestid);
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
							XSM_VERBOSE_OUTPUT("joinsession (XSMTS_JoinSession_SetHost) couldn't get session member list 0x%08x: request id %d\n", res, requestid);
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
							XSM_VERBOSE_OUTPUT("joinsession (XSMTS_JoinSession_SetHost) couldn't get session member list 0x%08x: request id %d\n", res, requestid);
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
								XSMtaskJoinSession* self = (XSMtaskJoinSession*)(asyncBlock->context);

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
										//self->SetState(XSMTS_JoinSession_ReportMatchSetup);
										self->waiting = false;

										XSM_VERBOSE_OUTPUT("joinsession (XSMTS_JoinSession_SetHost) write session succeeded: request id %d\n", self->requestid);

										XSM::OnSessionChanged(self->user_id, session);
									}
									else
									{
										// Handle failure
										XSM_VERBOSE_OUTPUT("joinsession (XSMTS_JoinSession_SetHost) write session failed: request id %d\n", self->requestid);

										HRESULT res;
										XUMuser* pUser = XUM::GetUserFromId(self->user_id);
										XblContextHandle xbl_context = NULL;
										if (pUser != NULL)
										{
											xbl_context = pUser->GetXboxLiveContext();
										}

										if ((pUser == NULL) || (xbl_context == NULL))
										{
											XSM_VERBOSE_OUTPUT("joinsession (XSMTS_JoinSession_SetHost) user (%d) for session has been lost: request id %d\n", self->user_id, self->requestid);
											self->SetState(XSMTS_JoinSession_FailureToWrite);
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
												XSMtaskJoinSession* self = (XSMtaskJoinSession*)(asyncBlock->context);

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

														DebugConsoleOutput("joinsession (XSMTS_JoinSession_SetHost) get session succeeded: request id %d, session name %s\n", self->requestid, pSessionName);
														YYFree(pSessionName);
#endif
														// Replace old session with new one
														XSM::OnSessionChanged(self->user_id, newsession);

														// Don't change state - we'll re-evaluate the host situation
														self->waiting = false;
													}
													else
													{
														XSM_VERBOSE_OUTPUT("findsession (XSMTS_FindSession_SetHost) get session failed 0x%08x: request id %d\n", res, self->requestid);
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
												XSM_VERBOSE_OUTPUT("joinsession (XSMTS_JoinSession_SetHost) get session failed 0x%08x: request id %d\n", res, self->requestid);
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

						if (FAILED(res))
						{
							XSM_VERBOSE_OUTPUT("joinsession (XSMTS_JoinSession_SetHost) write session failed: request id %d\n", requestid);
							SetState(XSMTS_JoinSession_FailureToWrite);
							waiting = false;
						}
						else
						{
							waiting = true;	// set to waiting
						}
					}

				}
			}


		} break;

		case XSMTS_JoinSession_ReportMatchSetup:
		{
			XSM_VERBOSE_OUTPUT("joinsession (XSMTS_JoinSession_ReportMatchSetup): request id %d\n", requestid);

			if (reFindingHost == false)
			{
				// Fire off an event to the user, with the match details
				SignalSuccess();
			}

			// Now gather the initial member details
			SetState(XSMTS_JoinSession_GetInitialMemberDetails);
			waiting = false;

		} break;

		case XSMTS_JoinSession_GetInitialMemberDetails:
		{
			XSM_VERBOSE_OUTPUT("joinsession (XSMTS_JoinSession_GetInitialMemberDetails): request id %d\n", requestid);

#if 0
			// We can't set up any connections at the moment, so skip this bit
			SetState(XSMTS_JoinSession_Completed);
			waiting = false;
#else
			// If we're the host, we want to set up secure device associations to all the other machines in the session
			// If we're one of the clients, we want to wait until we have a secure device association from the host before continuing

			const XblMultiplayerSessionProperties* props = XblMultiplayerSessionSessionProperties(session->session_handle);
			if (props == NULL)
			{
				XSM_VERBOSE_OUTPUT("joinsession (XSMTS_JoinSession_GetInitialMemberDetails) couldn't get session properties: request id %d\n", requestid);
				SetState(XSMTS_FailureCleanup);
				return;
			}

			const XblMultiplayerSessionMember* memberList = NULL;
			size_t memberCount = 0;
			res = XblMultiplayerSessionMembers(session->session_handle, &memberList, &memberCount);
			if (FAILED(res))
			{
				XSM_VERBOSE_OUTPUT("joinsession (XSMTS_JoinSession_GetInitialMemberDetails) XblMultiplayerSessionMembers() failed with error 0x%08x: request id %d\n", res, requestid);
				SetState(XSMTS_FailureCleanup);
				return;
			}

			// First, determine if we are the host
			bool IsHost = XSM::IsUserHost(user_id, session);

			if (IsHost)
			{
				XSM_VERBOSE_OUTPUT("joinsession (XSMTS_JoinSession_GetInitialMemberDetails) IsHost=true : request id %d\n", requestid);
				// Iterate through the member list for the session
				// Do not return the current user
				// Return other users that share the same device token as the current user (these will be on the same machine) but indicate that they are local
				// This way the game can avoid creating network connections for these users

				// Get the device token for this user
				// Since we're the host, we can just read it from the session properties
				const char* localDeviceToken = props->HostDeviceToken.Value;

				for (int i = 0; i < memberCount; i++)
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

				SetState(XSMTS_JoinSession_Completed);
				waiting = false;
			}
			else
			{
				XSM_VERBOSE_OUTPUT("joinsession (XSMTS_JoinSession_GetInitialMemberDetails) IsHost=false : request id %d\n", requestid);
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
					XSM_VERBOSE_OUTPUT("joinsession (XSMTS_JoinSession_GetInitialMemberDetails) No host found in session : request id %d\n", requestid);
					// Set state back to SetHost
					SetState(XSMTS_JoinSession_SetHost);
					reFindingHost = true;
					waiting = false;
				}
				else
				{
#ifdef REPORT_SESSION_JOINED_FOR_ALL_MEMBERS
					uint64 host_id = XSM::GetHost(session);

					if (host_id == 0)
					{
						XSM_VERBOSE_OUTPUT("findsession (XSMTS_JoinSession_GetInitialMemberDetails) host connection failed : request id %d\n", requestid);
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

					XSM_VERBOSE_OUTPUT("joinsession (XSMTS_JoinSession_GetInitialMemberDetails) received host connection : request id %d\n", requestid);

					// That's it
					SetState(XSMTS_JoinSession_Completed);
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
							extern void CreateChatConnectionForSessionMember(MultiplayerSessionMember ^ _member);
							CreateChatConnectionForSessionMember(session->Members->GetAt(i));
						}
#endif
					}

					if (host != nullptr)
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

						XSM_VERBOSE_OUTPUT("joinsession (XSMTS_JoinSession_GetInitialMemberDetails) received host connection : request id %d\n", requestid);
						// That's it
						SetState(XSMTS_JoinSession_Completed);
						waiting = false;
					}
					else
					{
						XSM_VERBOSE_OUTPUT("joinsession (XSMTS_JoinSession_GetInitialMemberDetails) host connection failed : request id %d\n", requestid);
						SetState(XSMTS_FailureCleanup);
						waiting = false;										
					}
#endif
				}
			}
#endif
		} break;

		case XSMTS_JoinSession_Completed:
		{
			XSM_VERBOSE_OUTPUT("joinsession (XSMTS_JoinSession_Completed): request id %d\n", requestid);

			SetState(XSMTS_Finished);
		} break;

		case XSMTS_JoinSession_NoSessionsFound:
		{
			XSM_VERBOSE_OUTPUT("joinsession (XSMTS_JoinSession_NoSessionsFound): request id %d\n", requestid);
			// Leave the session
			res = XblMultiplayerSessionLeave(session->session_handle);
			if (FAILED(res))
			{
				XSM_VERBOSE_OUTPUT("joinsession (XSMTS_JoinSession_NoSessionsFound) couldn't leave session: request id %d\n", requestid);
				// Not sure if there's anything else I can do at this point
			}

			// Write session
			res = WriteSessionAsync(xbl_context, session->session_handle, XblMultiplayerSessionWriteMode::UpdateExisting,
				[](XAsyncBlock* asyncBlock)
				{
					XSMtaskJoinSession* self = (XSMtaskJoinSession*)(asyncBlock->context);

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

						XSM_VERBOSE_OUTPUT("joinsession (XSMTS_JoinSession_NoSessionsFound) write succeeded: request id %d\n", self->requestid);
					}

					delete asyncBlock;
				});

			if (FAILED(res))
			{
				SignalNoSessionsFound();

				XSM::DeleteSessionGlobally(session);

				XSM_VERBOSE_OUTPUT("joinsession (XSMTS_JoinSession_NoSessionsFound) write failed: request id %d\n", requestid);
			}
			else
			{
				waiting = true;
			}
		} break;

		case XSMTS_FailureCleanup:
		{
			XSM_VERBOSE_OUTPUT("joinsession (XSMTS_FailureCleanup): request id %d\n", requestid);
			// Leave the session
			res = XblMultiplayerSessionLeave(session->session_handle);
			if (FAILED(res))
			{
				XSM_VERBOSE_OUTPUT("joinsession (XSMTS_FailureCleanup) exception (0x%08x): request id %d\n", res, requestid);
				// Not sure if there's anything else I can do at this point
			}

			// Write session
			res = WriteSessionAsync(xbl_context, session->session_handle, XblMultiplayerSessionWriteMode::UpdateExisting,
				[](XAsyncBlock* asyncBlock)
				{
					XSMtaskJoinSession* self = (XSMtaskJoinSession*)(asyncBlock->context);

					if (self->state == XSMTS_Finished)
					{
						// This task has been terminated, so do nothing		
						DebugConsoleOutput("joinsession (XSMTS_FailureCleanup) already in finished state\n");
					}
					else
					{
						// Regardless of what happened, signal destroyed and bail						
						XblMultiplayerWriteSessionResult(asyncBlock, NULL);		// pass in NULL to clean up returned session immediately

						// Regardless of what happened, signal failure and bail
						self->SignalFailure();

						XSM::DeleteSessionGlobally(self->session);

						XSM_VERBOSE_OUTPUT("joinsession (XSMTS_FailureCleanup) write succeeded: request id %d\n", self->requestid);
					}

					delete asyncBlock;
				});

			if (FAILED(res))
			{
				SignalFailure();

				XSM::DeleteSessionGlobally(session);

				XSM_VERBOSE_OUTPUT("joinsession (XSMTS_FailureCleanup) write failed: request id %d\n", requestid);
			}
			else
			{
				waiting = true;
			}
		} break;

		case XSMTS_JoinSession_FailureToWrite:
		{
			XSM_VERBOSE_OUTPUT("joinsession (XSMTS_JoinSession_FailureToWrite): request id %d\n", requestid);
			SignalDestroyed();

			XSM::DeleteSessionGlobally(session);			

		} break;
	}
}

void XSMtaskJoinSession::ProcessSessionChanged(xbl_session_ptr _updatedsession)
{	
	// Some common stuff
	XUMuser* pUser = XUM::GetUserFromId(user_id);
	XblContextHandle xbl_context = NULL;
	if (pUser != NULL)
	{
		xbl_context = pUser->GetXboxLiveContext();
	}

	// Note to self: Probably best to invert this logic so we check the changes first then test which state we are in
	// (A little worried about missing changes that occur if we are not in the correct state to receive them)
	XblMultiplayerSessionChangeTypes changes = XblMultiplayerSessionCompare(_updatedsession->session_handle, session->session_handle);
	if (state > 0)
	{
		DebugConsoleOutput("Join in state %s changes %d\n", names[state - 1], changes);
	}

	switch(state)
	{
		case XSMTS_JoinSession_WaitMatchTicketResult:
		{								
		} break;		

		case XSMTS_JoinSession_QOS:
		{
			XSM_VERBOSE_OUTPUT("joinsession (XSMTS_JoinSession_QOS): request id %d\n", requestid);
			// Compare the old and new sessions
			//MultiplayerSessionChangeTypes changes = MultiplayerSession::CompareMultiplayerSessions( _updatedsession, session );

			if ((changes & XblMultiplayerSessionChangeTypes::InitializationStateChange) == XblMultiplayerSessionChangeTypes::InitializationStateChange)
			{
				XSM_VERBOSE_OUTPUT("joinsession (XSMTS_JoinSession_QOS) initialisation state change: request id %d\n", requestid);

				const XblMultiplayerSessionInitializationInfo* initInfo = XblMultiplayerSessionGetInitializationInfo(_updatedsession->session_handle);
				if (initInfo == NULL)
				{
					XSM_VERBOSE_OUTPUT("joinsession (XSMTS_JoinSession_QOS) couldn't get initialisation info: request id %d\n", requestid);
					SetState(XSMTS_FailureCleanup);
					waiting = false;
				}
				else
				{
					if (initInfo->Episode == 0)
					{
						// QOS phase complete
						SetState(XSMTS_JoinSession_SetHost);
						waiting = false;

						XSM_VERBOSE_OUTPUT("QOS completed\n", requestid);
					}
					else
					{
						XblMultiplayerInitializationStage stage = initInfo->Stage;

						// I think we might only need to consider the 'measuring' state if we're relying on the MPSD service to do the QOS evaluation
						switch (stage)
						{
						case XblMultiplayerInitializationStage::None:
						{
							XSM_VERBOSE_OUTPUT("stage None\n", requestid);
						} break;

						case XblMultiplayerInitializationStage::Unknown:
						{
							XSM_VERBOSE_OUTPUT("stage Unknown\n", requestid);
						} break;

						case XblMultiplayerInitializationStage::Joining:
						{
							XSM_VERBOSE_OUTPUT("stage Joining\n", requestid);
						} break;

						case XblMultiplayerInitializationStage::Measuring:
						{
							XSM_VERBOSE_OUTPUT("stage Measuring\n", requestid);
						} break;

						case XblMultiplayerInitializationStage::Evaluating:
						{
							XSM_VERBOSE_OUTPUT("stage Evaluating\n", requestid);
						} break;

						case XblMultiplayerInitializationStage::Failed:
						{
							XSM_VERBOSE_OUTPUT("stage Failed\n", requestid);
						} break;
						}
					}
				}
			}
			
		} break;

		default:
		{
			
		} break;
	}

	// Update session
	session = _updatedsession;
}

void XSMtaskJoinSession::ProcessSessionDeleted(xbl_session_ptr _sessiontodelete)
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