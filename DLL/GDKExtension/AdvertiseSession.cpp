#include "SessionManagement.h"
#include "GDKX.h"
//#include <collection.h> 
//#include "Files/Support/Support_Data_Structures.h"

#define TICKET_TIMEOUT	30			// 30 seconds

//using namespace Windows::Foundation;
//#ifndef WIN_UAP
//using namespace Windows::Xbox::Networking;
//using namespace Windows::Xbox::System;
//#endif
//using namespace Microsoft::Xbox::Services;
//using namespace Microsoft::Xbox::Services::Multiplayer;
//using namespace Microsoft::Xbox::Services::Matchmaking;

//extern Platform::String^ g_PrimaryServiceConfigId;

extern void CreateAsynEventWithDSMap(int dsmapindex, int event_index);

XSMtaskAdvertiseSession::~XSMtaskAdvertiseSession()
{
	if (hopperName)
		YYFree(hopperName);
	if (matchAttributes)
		YYFree(matchAttributes);
}

void XSMtaskAdvertiseSession::Process()
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
		case XSMTS_AdvertiseSession_CreateMatchTicket:
		{					
			//TimeSpan ticketWaitTimeout;
			//ticketWaitTimeout.Duration = TICKET_TIMEOUT * TICKS_PER_SECOND; 
			////ticketWaitTimeout.Duration = 5 * TICKS_PER_SECOND; // 60 sec timeout
			
			XAsyncBlock* asyncBlock = new XAsyncBlock();
			asyncBlock->queue = XSM::GetTaskQueue();
			asyncBlock->context = this;
			asyncBlock->callback = [](XAsyncBlock* asyncBlock)
			{
				XSMtaskAdvertiseSession* self = (XSMtaskAdvertiseSession*)(asyncBlock->context);

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
						DebugConsoleOutput("advertisesession (XSMTS_AdvertiseSession_CreateMatchTicket) create match ticket failed: error 0x%08x: request id %d\n", res, self->requestid);
#endif
						self->SetState(XSMTS_FailureCleanup);
						self->waiting = false;
					}
					else
					{
#ifdef XSM_VERBOSE_TRACE
						DebugConsoleOutput("advertisesession (XSMTS_AdvertiseSession_CreateMatchTicket) create match ticket succeeded: request id %d ticket id %s\n", self->requestid, self->ticketResponse.matchTicketId);
#endif

						self->SetState(XSMTS_AdvertiseSession_WaitMatchTicketResult);
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
				TICKET_TIMEOUT/* * TICKS_PER_SECOND*/,		// timeouts are apparently measured in seconds now - see https://forums.xboxlive.com/questions/99245/xblmatchmakingcreatematchticketasync%E3%81%AEtickettimeout.html
				XblPreserveSessionMode::Always,
				matchAttributes,
				asyncBlock);

			if (FAILED(res))
			{
#ifdef XSM_VERBOSE_TRACE
				DebugConsoleOutput("advertisesession (XSMTS_AdvertiseSession_CreateMatchTicket) create match ticket failed: request id %d\n", requestid);
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
		
		case XSMTS_AdvertiseSession_WaitForAvailableSlots:
		{
			const XblMultiplayerSessionMember* memberList = NULL;
			size_t memberCount = 0;

			res = XblMultiplayerSessionMembers(session->session_handle, &memberList, &memberCount);
			if (FAILED(res))
			{
#ifdef XSM_VERBOSE_TRACE
				DebugConsoleOutput("advertisesession (XSMTS_AdvertiseSession_WaitForAvailableSlots) couldn't get member list: request id %d\n", requestid);
#endif
				SetState(XSMTS_FailureCleanup);
			}
			else
			{
				const XblMultiplayerSessionConstants* consts = XblMultiplayerSessionSessionConstants(session->session_handle);
				if (consts == NULL)
				{
#ifdef XSM_VERBOSE_TRACE
					DebugConsoleOutput("advertisesession (XSMTS_AdvertiseSession_WaitForAvailableSlots) couldn't get session constants: request id %d\n", requestid);
#endif
					SetState(XSMTS_FailureCleanup);
				}
				else
				{
					// Check current session status and see if there are any available member slots - if so create a new match ticket
					if (memberCount < consts->MaxMembersInSession)
					{
						SetState(XSMTS_AdvertiseSession_CreateMatchTicket);
					}
				}				
			}
		} break;

		case XSMTS_FailureCleanup:
		{
			// Leave the session
			res = XblMultiplayerSessionLeave(session->session_handle);
			if (FAILED(res))
			{
#ifdef XSM_VERBOSE_TRACE
				DebugConsoleOutput("advertisesession (XSMTS_FailureCleanup) couldn't leave session: request id %d\n", requestid);
#endif
				// Not sure if there's anything else I can do at this point
			}

			// Write session
			res = WriteSessionAsync(xbl_context, session->session_handle, XblMultiplayerSessionWriteMode::UpdateExisting,
				[](XAsyncBlock* asyncBlock)
				{
					XSMtaskAdvertiseSession* self = (XSMtaskAdvertiseSession*)(asyncBlock->context);

					if (self->state == XSMTS_Finished)
					{
						// This task has been terminated, so do nothing						
					}
					else
					{
						// Regardless of what happened, signal destroyed and bail
						XblMultiplayerSessionHandle sessionHandle;
						//xbl_session_ptr session = std::make_shared<xbl_session>(sessionHandle);
						//XblMultiplayerWriteSessionResult(asyncBlock, &(session->session_handle));
						XblMultiplayerWriteSessionResult(asyncBlock, &sessionHandle);

						self->SignalDestroyed();

						XSM::DeleteSessionGlobally(self->session);
					}

					delete asyncBlock;
				});

			if (FAILED(res))
			{
				SignalFailure();

				XSM::DeleteSessionGlobally(session);
			}
			else
			{
				waiting = true;
			}
		} break;

		case XSMTS_AdvertiseSession_FailureToWrite:
		{
			SignalDestroyed();

			XSM::DeleteSessionGlobally(session);			

			//XSMsession^ xsmsession = XSM::GetSession(session);
			//XSM::DeleteSession(xsmsession->id);			
		} break;
	}
}

void XSMtaskAdvertiseSession::ProcessSessionChanged(xbl_session_ptr _updatedsession)
{	
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
		case XSMTS_AdvertiseSession_WaitMatchTicketResult:
		{						
#ifdef XSM_VERBOSE_TRACE
			DebugConsoleOutput("advertisesession (XSMTS_AdvertiseSession_WaitMatchTicketResult): request id %d\n", requestid);
#endif
			// Compare the old and new sessions
			XblMultiplayerSessionChangeTypes changes = XblMultiplayerSessionCompare(_updatedsession->session_handle, session->session_handle);			

			if ((changes & XblMultiplayerSessionChangeTypes::MatchmakingStatusChange) == XblMultiplayerSessionChangeTypes::MatchmakingStatusChange)
			{
#ifdef XSM_VERBOSE_TRACE
				DebugConsoleOutput("advertisesession (XSMTS_AdvertiseSession_WaitMatchTicketResult) matchmaking status change: request id %d\n", requestid);
#endif
				const XblMultiplayerMatchmakingServer* mmstatus = XblMultiplayerSessionMatchmakingServer(_updatedsession->session_handle);
				if (mmstatus == NULL)
				{
#ifdef XSM_VERBOSE_TRACE
					DebugConsoleOutput("advertisesession (XSMTS_AdvertiseSession_WaitMatchTicketResult) couldn't get matchmaking server data: request id %d\n", requestid);
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
						DebugConsoleOutput("expired\n");
#endif
						// Resubmit the match ticket?
						// How does this affect timeouts etc?

						// Delete the match ticket (don't wait for this to complete)
						DeleteMatchTicketAsync(xbl_context, g_XboxSCID, hopperName, ticketResponse.matchTicketId);												

						SetState(XSMTS_AdvertiseSession_CreateMatchTicket);
						waiting = false;
					}
					else if (mmstatus->Status == XblMatchmakingStatus::Found)
					{
						// Delete the match ticket (don't wait for this to complete)
						DeleteMatchTicketAsync(xbl_context, g_XboxSCID, hopperName, ticketResponse.matchTicketId);

						// Found a session (should in fact be the same one we created at the start)

						// Should we remove the old session, then add this one? Or just silently replace the old one?
						// How do we handle cases where other operations are referencing the same session? Ref count?
						// What happens if an event occurs referencing the old session?

						// We actually need to replace the session here because the user has been returned a handle to it						
						//MultiplayerSession^ resultSession = ref new MultiplayerSession(user->GetXboxLiveContext(), sessionRef);
						//WriteSessionSync( xboxLiveContext, resultSession, MultiplayerSessionWriteMode::UpdateOrCreateNew );
						//matchmakingComplete = true;								

						XAsyncBlock* asyncBlock = new XAsyncBlock();
						asyncBlock->queue = XSM::GetTaskQueue();
						asyncBlock->context = this;
						asyncBlock->callback = [](XAsyncBlock* asyncBlock)
						{
							XSMtaskAdvertiseSession* self = (XSMtaskAdvertiseSession*)(asyncBlock->context);
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

									// Check to see if we've hit our player limit
									const XblMultiplayerSessionMember* memberList = NULL;
									size_t memberCount = 0;

									res = XblMultiplayerSessionMembers(newsession->session_handle, &memberList, &memberCount);
									if (FAILED(res))
									{
#ifdef XSM_VERBOSE_TRACE
										DebugConsoleOutput("advertisesession (XSMTS_AdvertiseSession_WaitMatchTicketResult) XblMultiplayerSessionMembers failed 0x%08x: request id %d\n", res, self->requestid);
#endif
										self->SetState(XSMTS_FailureCleanup);
									}
									else
									{
										const XblMultiplayerSessionConstants* consts = XblMultiplayerSessionSessionConstants(newsession->session_handle);
										if (consts == NULL)
										{
#ifdef XSM_VERBOSE_TRACE
											DebugConsoleOutput("advertisesession (XSMTS_AdvertiseSession_WaitMatchTicketResult) couldn't get session constants: request id %d\n", self->requestid);
#endif
											self->SetState(XSMTS_FailureCleanup);
										}
										else
										{
											// Check current session status and see if there are any available member slots - if so create a new match ticket
											if (memberCount < consts->MaxMembersInSession)
											{
												self->SetState(XSMTS_AdvertiseSession_CreateMatchTicket);	// create another match ticket
											}
											else
											{
												self->SetState(XSMTS_AdvertiseSession_WaitForAvailableSlots);	// wait for available slots
											}
										}
									}

									// This is an update to the existing session
									//XSM::ReplaceSession(session, newSession);
									XSM::OnSessionChanged(0, newsession);

									self->waiting = false;
								}
								else
								{
#ifdef XSM_VERBOSE_TRACE
									DebugConsoleOutput("advertisesession (XSMTS_AdvertiseSession_WaitMatchTicketResult) XblMultiplayerGetSessionResult failed 0x%08x: request id %d\n", res, self->requestid);
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
					}
					else
					{
						// Delete the match ticket (don't wait for this to complete)
						DeleteMatchTicketAsync(xbl_context, g_XboxSCID, hopperName, ticketResponse.matchTicketId);						

						// Try another match ticket
						SetState(XSMTS_AdvertiseSession_CreateMatchTicket);
						waiting = false;
					}

					// Update session
					session = _updatedsession;
				}
			}							
		
		} break;

		default:
		{
			session = _updatedsession;
		} break;
	}
}

void XSMtaskAdvertiseSession::ProcessSessionDeleted(xbl_session_ptr _sessiontodelete)
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