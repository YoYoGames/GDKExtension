#include "GDKX.h"
#include "SessionManagement.h"
#include "PlayFabPartyManagement.h"
//#include <collection.h> 
//#include "Files/Support/Support_Data_Structures.h"


//using namespace Windows::Foundation;
using namespace Party;
//using namespace Windows::Xbox::Networking;
//#ifndef WIN_UAP
//using namespace Windows::Xbox::System;
//#endif
//using namespace Microsoft::Xbox::Services;
//using namespace Microsoft::Xbox::Services::Multiplayer;
//using namespace Microsoft::Xbox::Services::Matchmaking;

//extern void CreateAsynEventWithDSMap(int dsmapindex, int event_index);

XSMtaskCreateSession::~XSMtaskCreateSession()
{
	if (hopperName)
		YYFree(hopperName);
	if (matchAttributes)
		YYFree(matchAttributes);
}

void XSMtaskCreateSession::Process()
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

	if (xbl_context == NULL)
	{
		SetState(XSMTS_FailureCleanup);
		return;
	}

	switch(state)
	{
		case XSMTS_CreateSession_InitialSession:
		{
#ifdef XSM_VERBOSE_TRACE
			DebugConsoleOutput("createsession (XSMTS_CreateSession_InitialSession): request id %d\n", requestid);
#endif

			res = WriteSessionAsync(xbl_context, session->session_handle, XblMultiplayerSessionWriteMode::UpdateOrCreateNew,
				[](XAsyncBlock* asyncBlock)
				{					
					XSMtaskCreateSession* self = (XSMtaskCreateSession*)(asyncBlock->context);

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
							DebugConsoleOutput("createsession (XSMTS_CreateSession_InitialSession) write succeeded: request id %d\n", self->requestid);
#endif
							self->SetState(XSMTS_CreateSession_SetHost);

							XSM::OnSessionChanged(0, session);

							self->waiting = false;
						}
						else
						{
							// Handle failure
#ifdef XSM_VERBOSE_TRACE
							DebugConsoleOutput("createsession (XSMTS_CreateSession_InitialSession) write failed: request id %d\n", self->requestid);
#endif
							self->SetState(XSMTS_CreateSession_FailureToWrite);
							self->waiting = false;
						}
					}

					delete asyncBlock;
				});

			//res = XblMultiplayerWriteSessionAsync(xbl_context, session->session_handle, XblMultiplayerSessionWriteMode::UpdateOrCreateNew, asyncBlock.get());
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
				DebugConsoleOutput("createsession (XSMTS_CreateSession_InitialSession) write failed: request id %d\n", requestid);
#endif

				SetState(XSMTS_FailureCleanup);
				waiting = false;
			}

		} break;

		case XSMTS_CreateSession_SetHost:
		{
			// We should be the only member in this session, so just set ourselves as host, then recommit
			const XblMultiplayerSessionMember* userAsMember = NULL;
			const XblMultiplayerSessionMember* memberList = NULL;
			size_t memberCount = 0;

			res = XblMultiplayerSessionMembers(session->session_handle, &memberList, &memberCount);
			if (FAILED(res))
			{
#ifdef XSM_VERBOSE_TRACE
				DebugConsoleOutput("createsession (XSMTS_CreateSession_InitialSession) couldn't get member list: request id %d\n", requestid);
#endif
				SetState(XSMTS_FailureCleanup);
			}
			else
			{
				for (int i = 0; i < memberCount; i++)
				{
					if (memberList[i].Xuid == user_id)
					{
						userAsMember = &memberList[i];
						break;
					}
				}

				if (userAsMember != NULL)
				{
					XblMultiplayerSessionSetHostDeviceToken(session->session_handle, userAsMember->DeviceToken);

					// Write session
					res = WriteSessionAsync(xbl_context, session->session_handle, XblMultiplayerSessionWriteMode::UpdateExisting,
						[](XAsyncBlock* asyncBlock)
						{
							XSMtaskCreateSession* self = (XSMtaskCreateSession*)(asyncBlock->context);

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
									DebugConsoleOutput("createsession (XSMTS_CreateSession_SetHost) write session succeeded: request id %d\n", self->requestid);
#endif
									self->SetState(XSMTS_SetupPlayFabNetwork);//XSMTS_CreateSession_SetupPlayFabNetwork);//XSMTS_CreateSession_CreateSuccess;

									XSM::OnSessionChanged(self->user_id, session);

									self->waiting = false;
								}
								else
								{
									// Handle failure
#ifdef XSM_VERBOSE_TRACE
									DebugConsoleOutput("createsession (XSMTS_CreateSession_SetHost) write session failed: request id %d\n", self->requestid);
#endif
									//self->SetState(XSMTS_CreateSession_FailureToWrite);	// hmmm, why is this commented out in the XDK version
									self->waiting = false;
								}
							}

							delete asyncBlock;
						});

					if (SUCCEEDED(res))
					{
						waiting = true;
					}
					else
					{						
#ifdef XSM_VERBOSE_TRACE
						DebugConsoleOutput("createsession (XSMTS_CreateSession_SetHost) write session failed with nullptr: request id %d\n", requestid);
#endif

						SetState(XSMTS_CreateSession_FailureToWrite);
						waiting = false;
					}
				}
			}

		} break;

		case XSMTS_ConnectionToPlayFabNetworkCompleted:
		{
			// This is the exit point from the standard connect-to-playfab-network operation
			// Immediately transition to our next state
			SetState(XSMTS_CreateSession_CreateSuccess);//XSMTS_CreateSession_WritePlayFabDetailsToSession);
		} break;

		case XSMTS_CreateSession_CreateSuccess:
		{
			// Report successful session creation to the user
			// When creating a session in this fashion we don't want to wait for the subsequent match ticket to complete
			XSMsession* xsmsession = XSM::GetSession(session);
			//xsmsession->network = network;
			strcpy(xsmsession->playfabNetworkIdentifier, networkDescriptor.networkIdentifier);
			int sessionid = xsmsession->id;

			const XblMultiplayerSessionInfo* session_info = XblMultiplayerSessionGetInfo(session->session_handle);			

			int dsMapIndex = CreateDsMap(6, "id",(double)MATCHMAKING_SESSION,(void *)NULL,
			"status", 0.0, "session_created",
			//"session_id", (double) respData->roomDataInternal->roomId, NULL,
			"sessionid", (double) sessionid, NULL,
			"requestid", (double) requestid, NULL,
			"correlationid", (double)0, session_info->CorrelationId,
			"error", 0.0, NULL);	

			CreateAsyncEventWithDSMap(dsMapIndex,EVENT_OTHER_SOCIAL);			

			// Right crank up a session advertisement operation
			XSMtaskAdvertiseSession* adtask = new XSMtaskAdvertiseSession();
			adtask->session = session;
			adtask->user_id = user_id;
			adtask->state = XSMTS_AdvertiseSession_CreateMatchTicket;
			adtask->hopperName = (char*)YYStrDup(hopperName);
			adtask->matchAttributes = (char*)YYStrDup(matchAttributes);
			adtask->requestid = requestid;			// just inherit the request ID of this context			

			XSM::AddTask(adtask);

			SetState(XSMTS_Finished);			// okay, we're done			
			waiting = false;

		} break;
		
		case XSMTS_FailureCleanup:
		{
			res = XblMultiplayerSessionLeave(session->session_handle);
			if (FAILED(res))
			{
#ifdef XSM_VERBOSE_TRACE
				DebugConsoleOutput("createsession (XSMTS_CreateSession_FailureCleanup) couldn't leave session: request id %d\n", requestid);
#endif
				// Not sure if there's anything else I can do at this point
			}

			// Write session
			res = WriteSessionAsync(xbl_context, session->session_handle, XblMultiplayerSessionWriteMode::UpdateExisting,
				[](XAsyncBlock* asyncBlock)
				{
					XSMtaskCreateSession* self = (XSMtaskCreateSession*)(asyncBlock->context);

					if (self->state == XSMTS_Finished)
					{
						// This task has been terminated, so do nothing						
					}
					else
					{
						// Regardless of what happened, signal destroyed and bail
						XblMultiplayerWriteSessionResult(asyncBlock, NULL);		// pass in NULL to clean up returned session immediately

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

		case XSMTS_CreateSession_FailureToWrite:
		{
			SignalDestroyed();

			XSM::DeleteSessionGlobally(session);			

			//XSMsession^ xsmsession = XSM::GetSession(session);
			//XSM::DeleteSession(xsmsession->id);			
		} break;
	}
}

void XSMtaskCreateSession::ProcessSessionChanged(xbl_session_ptr _updatedsession)
{	
	switch(state)
	{
		default:
		{
			session = _updatedsession;
		} break;
	}
}

void XSMtaskCreateSession::ProcessSessionDeleted(xbl_session_ptr _sessiontodelete)
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

void XSMtaskCreateSession::ProcessPlayFabPartyChange(const PartyStateChange* _change)
{
	XSMtaskBase::ProcessPlayFabPartyChange(_change);

	if (_change == NULL)
		return;

	switch (state)
	{
		default: break;
	}
}
