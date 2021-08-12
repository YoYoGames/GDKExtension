//
// Copyright (C) 2020 Opera Norway AS. All rights reserved.
//
// This file is an original work developed by Opera.
//

#include "GDKX.h"
#include "PlayFabPartyManagement.h"
#include "SessionManagement.h"
#include "UserManagement.h"

/* Defined by Windows headers, conflicts with PartyImpl.h */
#undef SendMessage
#include "PartyImpl.h"

#include "PartyXboxLiveImpl.h"
#include <stdint.h>
#include <string>
#include <ctime>

#define PACKETBUFFER_INACTIVE_USER_TIMEOUT	60 * 1000 * 1000

#ifdef XSM_VERBOSE_TRACE
#define PARTY_DBG_TRACE(a,...) DebugConsoleOutput(a, __VA_ARGS__)
#else
#define PARTY_DBG_TRACE(a,...)
#endif

using namespace Party;

PartyString GetErrorMessage(PartyError error);
PartyString GetXblErrorMessage(PartyError error);

struct SetupLocalUserPayload : public PlayFabPayloadBase
{
	SetupLocalUserPayload() : PlayFabPayloadBase(PlayFabPayloadType::SetupLocalUser), requestID(-1), user_id(0), playfabLocalChatUser(NULL), playfabLocalChatControl(NULL), playfabLocalUser(NULL), playfabTokenExpiryTime(-1) {}

	void Free(const char* _contextstring)
	{		
		PartyError err;
		if (playfabLocalChatControl != NULL)
		{
			PartyLocalDevice* localDevice = NULL;
			err = PartyManager::GetSingleton().GetLocalDevice(&localDevice);
			if (PARTY_FAILED(err))
			{
				PARTY_DBG_TRACE("%s: GetLocalDevice() failed with error %s for user %llu\n", _contextstring, GetErrorMessage(err), user_id);
			}
			else
			{
				err = localDevice->DestroyChatControl(playfabLocalChatControl, NULL);
				if (PARTY_FAILED(err))
				{
					PARTY_DBG_TRACE("%s: DestroyChatControl() failed with error %s for user %llu\n", _contextstring, GetErrorMessage(err), user_id);
				}
				else
				{
					playfabLocalChatControl = NULL;
				}
			}
		}

		if (playfabLocalUser != NULL)
		{
			err = PartyManager::GetSingleton().DestroyLocalUser(playfabLocalUser, NULL);
			if (PARTY_FAILED(err))
			{
				PARTY_DBG_TRACE("%s: DestroyLocalUser() failed with error %s for user %llu\n", _contextstring, GetErrorMessage(err), user_id);
			}
			else
			{
				playfabLocalUser = NULL;
			}
		}

		if (playfabLocalChatUser != NULL)
		{
			err = PartyXblManager::GetSingleton().DestroyChatUser(playfabLocalChatUser);
			if (PARTY_FAILED(err))
			{
				PARTY_DBG_TRACE("%s: DestroyChatUser() failed with error %s for user %llu\n", _contextstring, GetXblErrorMessage(err), user_id);
			}
			else
			{
				playfabLocalChatUser = NULL;
			}
		}
	}

	int requestID;
	uint64_t user_id;
	PartyXblLocalChatUser* playfabLocalChatUser;
	PartyLocalChatControl* playfabLocalChatControl;
	PartyLocalUser* playfabLocalUser;	
	time_t playfabTokenExpiryTime;
};

int PlayFabPartyManager::m_initRefCount = 0;
std::unordered_map<std::string, std::unique_ptr<PacketBufferNetwork>> PlayFabPartyManager::packetBuffer;
std::unordered_map<std::string, std::unique_ptr<NetworkUserInfo>> PlayFabPartyManager::networkUsers;

const char* g_PartyTitleID = "B07D0";

PartyString GetErrorMessage(PartyError error)
{
	PartyString errString = NULL;

	PartyError err = PartyManager::GetErrorMessage(error, &errString);
	if (PARTY_FAILED(err))
	{
		PARTY_DBG_TRACE("Failed to get PlayFab Party error message %d\n", error);
		return "[ERROR]";
	}

	return errString;
}

PartyString GetXblErrorMessage(PartyError error)
{
	PartyString errString = NULL;

	PartyError err = PartyXblManager::GetErrorMessage(error, &errString);
	if (PARTY_FAILED(err))
	{
		PARTY_DBG_TRACE("Failed to get PlayFab Party Xbox Live Helper error message %d\n", error);
		return "[XBLERROR]";
	}

	return errString;
}

struct NetworkUserInfo
{
	char networkIdentifier[Party::c_networkIdentifierStringLength + 1];
	std::unordered_map<std::string, uint64_t> users;

	NetworkUserInfo()
	{
		networkIdentifier[0] = '\0';
	}
};

struct PacketBufferData
{
	char receivingEntityID[Party::c_maxEntityIdStringLength + 1];

	void* messageBuffer;
	uint32_t messageSize;

	PacketBufferData* pNext;
	PacketBufferData* pPrev;

	PacketBufferData() : messageBuffer(NULL), messageSize(0), pNext(NULL), pPrev(NULL) { receivingEntityID[0] = '\0'; }
	PacketBufferData(const char* _receivingEntityID, const void* _messageBuffer, uint32_t _messageSize)
	{
		strcpy(receivingEntityID, _receivingEntityID);
		messageBuffer = YYAlloc(_messageSize);
		memcpy(messageBuffer, _messageBuffer, _messageSize);
		messageSize = _messageSize;

		pNext = NULL;
		pPrev = NULL;
	}
	~PacketBufferData()
	{
		YYFree(messageBuffer);
	}
};

struct PacketBufferUser
{
	char entityID[Party::c_maxEntityIdStringLength + 1];	// don't really need this as it's also the hash table key, but it's useful for debugging
	int64_t firstpackettime;

	PacketBufferData* pHead;
	PacketBufferData* pTail;

	PacketBufferUser() : pHead(NULL), pTail(NULL), firstpackettime(-1) { entityID[0] = '\0'; }
	~PacketBufferUser()
	{
		PacketBufferData* pCurr = pHead;
		while (pCurr != NULL)
		{
			PacketBufferData* pNext = pCurr->pNext;
			delete pCurr;

			pCurr = pNext;
		}
	}

	void AddPacket(const char* _receivingEntityID, const void* _messageBuffer, uint32_t _messageSize)
	{
		PacketBufferData* newData = new PacketBufferData(_receivingEntityID, _messageBuffer, _messageSize);
		if (pHead == NULL)
		{
			pHead = pTail = newData;
		}
		else
		{
			newData->pPrev = pTail;
			pTail->pNext = newData;
			pTail = newData;
		}
	}

	void DispatchPacket()
	{
		PacketBufferData* pCurr = pHead;
		if (pHead != NULL)
		{
			PacketBufferData* pNext = pCurr->pNext;

			uint64_t receivingUserID = PlayFabPartyManager::GetUserFromEntityID(pCurr->receivingEntityID);

			// FIXME

#if 0
			// Trigger an async event for each endpoint			
			int bufferindex = CreateBuffer(pCurr->messageSize, eBuffer_Format_Fixed, 1);
			IBuffer* pBuff = GetIBuffer(bufferindex);
			memcpy(pBuff->GetBuffer(), pCurr->messageBuffer, pCurr->messageSize);

			int dsMapIndex = GDKX::CreateDsMap(0);

			GDKX::DsMapAddDouble(dsMapIndex, "type", eNetworkingEventType_IncomingData);
			GDKX::DsMapAddString(dsMapIndex, "id", entityID);			// Server that's throwing the event
			GDKX::DsMapAddDouble(dsMapIndex, "buffer", bufferindex);
			GDKX::DsMapAddDouble(dsMapIndex, "size", pCurr->messageSize);

			GDKX::DsMapAddString(dsMapIndex, "ip", entityID);		// connecting socket
			GDKX::DsMapAddDouble(dsMapIndex, "port", 0.0);			// connecting socket

			GDKX::DsMapAddString(dsMapIndex, "receivingID", pCurr->receivingEntityID);		// receiving endpoint
			GDKX::DsMapAddDouble(dsMapIndex, "receivingUser", receivingUserID);		// receiving endpoint

			CreateAsynEventWithDSMapAndBuffer(dsMapIndex, bufferindex, EVENT_OTHER_WEB_NETWORKING);
#endif

			delete pCurr;

			pHead = pNext;
			if (pHead == NULL)
			{
				pTail = NULL;
			}
		}
	}
};

struct PacketBufferNetwork
{
	char networkIdentifier[Party::c_networkIdentifierStringLength + 1];
	std::unordered_map<std::string, std::unique_ptr<PacketBufferUser>> users;

	PacketBufferNetwork()
	{
		networkIdentifier[0] = '\0';
	}

	void AddPacket(const char* _sendingEntityID, const char* _receivingEntityID, const void* _messageBuffer, uint32_t _messageSize)
	{
		auto iUser = users.find(_sendingEntityID);
		if (iUser == users.end())
		{
			PacketBufferUser* newUser = new PacketBufferUser();
			strcpy(newUser->entityID, _sendingEntityID);
			newUser->firstpackettime = Timing_Time();

			bool inserted;
			std::tie(iUser, inserted) = users.emplace(_sendingEntityID, std::unique_ptr<PacketBufferUser>(newUser));
		}

		iUser->second->AddPacket(_receivingEntityID, _messageBuffer, _messageSize);
	}

	void PurgeInactiveUsers(NetworkUserInfo* _networkUsers)
	{
		if (_networkUsers == NULL)
			return;

		int64_t currtime = Timing_Time();

		for (auto iUser = users.begin(); iUser != users.end();)
		{
			PacketBufferUser* pUser = iUser->second.get();

			if (pUser != NULL)
			{
				if (_networkUsers->users.find(iUser->first) != _networkUsers->users.end())		// anything in the network user list will get dispatched by DispatchPackets()
				{
					int64_t elapsedtime = currtime - pUser->firstpackettime;
					if (elapsedtime > PACKETBUFFER_INACTIVE_USER_TIMEOUT)
					{
						iUser = users.erase(iUser);
						continue;
					}
				}
			}

			++iUser;
		}
	}

	void DispatchPackets(NetworkUserInfo* _networkUsers)
	{
		if (_networkUsers == NULL)
			return;

		for (auto iUser = users.begin(); iUser != users.end();)
		{
			PacketBufferUser* pUser = iUser->second.get();

			if (pUser != NULL)
			{
				if (_networkUsers->users.find(iUser->first) != _networkUsers->users.end())		// we don't care what the value of the bool is in the hash table - if the user exists then we know we shouldn't keep buffering its packets
				{
					// Dispatch a single packet for this user - we're doing one per frame so we're not at the mercy of the async event execution order
					pUser->DispatchPacket();

					if (pUser->pHead == NULL)
					{
						// All out of packets, so scrub this user
						iUser = users.erase(iUser);
						continue;
					}
				}
			}

			++iUser;
		}
	}
};

bool PlayFabPartyManager::Init()
{
	if (m_initRefCount == 0)
	{
		PartyError err;
		
		err = PartyManager::GetSingleton().Initialize(g_PartyTitleID);
		if (PARTY_FAILED(err))
		{
			PARTY_DBG_TRACE("PlayFabPartyManager::Init(): Couldn't get the party started %hs\n", GetErrorMessage(err));
			return false;
		}

		err = PartyXblManager::GetSingleton().Initialize(g_PartyTitleID);
		if (PARTY_FAILED(err))
		{
			PARTY_DBG_TRACE("PlayFabPartyManager::Init(): Couldn't get the chat going at the party %hs\n", GetXblErrorMessage(err));
			PartyManager::GetSingleton().Cleanup();
			return false;
		}
	}

	m_initRefCount++;

	return true;
}

void PlayFabPartyManager::Shutdown()
{
	m_initRefCount--;

	if (m_initRefCount == 0)
	{
		PartyManager::GetSingleton().Cleanup();
		PartyXblManager::GetSingleton().Cleanup();

		packetBuffer.clear();
		networkUsers.clear();
	}
	else if (m_initRefCount < 0)
	{
		PARTY_DBG_TRACE("PlayFabPartyManager::Shutdown(): refcount %d - too many shutdowns\n", m_initRefCount);

		m_initRefCount = 0;
	}
}

bool PlayFabPartyManager::IsInitialised()
{
	return m_initRefCount > 0;
}

void PlayFabPartyManager::Process()
{
	if (m_initRefCount <= 0)
		return;

	FlushBufferedPackets();

	uint32_t count;
	PartyXblStateChangeArray xblChanges;

	// Process Xbl messages
	PartyError err = PartyXblManager::GetSingleton().StartProcessingStateChanges(
		&count,
		&xblChanges
	);

	if (PARTY_FAILED(err))
	{
		PARTY_DBG_TRACE("PlayFabPartyManager::Process() - PartyXblManager::StartProcessingStateChanges() failed: %hs\n", GetXblErrorMessage(err));
		return;
	}

	for (uint32_t i = 0; i < count; i++)
	{
		const PartyXblStateChange* change = xblChanges[i];
		if (change)
		{
			switch (change->stateChangeType)
			{
			case PartyXblStateChangeType::CreateLocalChatUserCompleted: OnCreateLocalChatUserCompleted(change); break;
			//case PartyXblStateChangeType::LocalChatUserDestroyed: OnLocalChatUserDestroyed(change); break;
			case PartyXblStateChangeType::LoginToPlayFabCompleted: OnLoginToPlayFabCompleted(change); break;
			case PartyXblStateChangeType::RequiredChatPermissionInfoChanged: OnRequiredChatPermissionInfoChanged(change); break;
			//case PartyXblStateChangeType::TokenAndSignatureRequested: OnTokenAndSignatureRequested(change); break;
			}
		}
	}

	err = PartyXblManager::GetSingleton().FinishProcessingStateChanges(count, xblChanges);

	if (PARTY_FAILED(err))
	{
		PARTY_DBG_TRACE("PlayFabPartyManager::Process() - PartyXblManager::FinishProcessingStateChanges failed: %hs\n", GetXblErrorMessage(err));
		return;
	}


	PartyStateChangeArray changes;

	// Start processing messages from PlayFab Party
	err = PartyManager::GetSingleton().StartProcessingStateChanges(
		&count,
		&changes
	);

	if (PARTY_FAILED(err))
	{
		PARTY_DBG_TRACE("PlayFabPartyManager::Process() - PartyManager::StartProcessingStateChanges() failed: %hs\n", GetErrorMessage(err));
		return;
	}

	for (uint32_t i = 0; i < count; i++)
	{
		const PartyStateChange* change = changes[i];
		if (change)
		{
			switch (change->stateChangeType)
			{
			//case PartyStateChangeType::RegionsChanged: OnRegionsChanged(change); break;
			//case PartyStateChangeType::DestroyLocalUserCompleted: OnDestroyLocalUserCompleted(change); break;
			case PartyStateChangeType::CreateNewNetworkCompleted: OnCreateNewNetworkCompleted(change); break;
			case PartyStateChangeType::ConnectToNetworkCompleted: OnConnectToNetworkCompleted(change); break;
			case PartyStateChangeType::AuthenticateLocalUserCompleted: OnAuthenticateLocalUserCompleted(change); break;
			//case PartyStateChangeType::NetworkConfigurationMadeAvailable: OnNetworkConfigurationMadeAvailable(change); break;
			//case PartyStateChangeType::NetworkDescriptorChanged: OnNetworkDescriptorChanged(change); break;
			//case PartyStateChangeType::LocalUserRemoved: OnLocalUserRemoved(change); break;
			//case PartyStateChangeType::RemoveLocalUserCompleted: OnRemoveLocalUserCompleted(change); break;
			//case PartyStateChangeType::LocalUserKicked: OnLocalUserKicked(change); break;
			case PartyStateChangeType::CreateEndpointCompleted: OnCreateEndpointCompleted(change); break;
			//case PartyStateChangeType::DestroyEndpointCompleted: OnDestroyEndpointCompleted(change); break;
			//case PartyStateChangeType::EndpointCreated: OnEndpointCreated(change); break;
			//case PartyStateChangeType::EndpointDestroyed: OnEndpointDestroyed(change); break;
			//case PartyStateChangeType::RemoteDeviceCreated: OnRemoteDeviceCreated(change); break;
			//case PartyStateChangeType::RemoteDeviceDestroyed: OnRemoteDeviceDestroyed(change); break;
			//case PartyStateChangeType::RemoteDeviceJoinedNetwork: OnRemoteDeviceJoinedNetwork(change); break;
			//case PartyStateChangeType::RemoteDeviceLeftNetwork: OnRemoteDeviceLeftNetwork(change); break;
			//case PartyStateChangeType::DevicePropertiesChanged: OnDevicePropertiesChanged(change); break;
			//case PartyStateChangeType::LeaveNetworkCompleted: OnLeaveNetworkCompleted(change); break;
			//case PartyStateChangeType::NetworkDestroyed: OnNetworkDestroyed(change); break;
			case PartyStateChangeType::EndpointMessageReceived: OnEndpointMessageReceived(change); break;
			//case PartyStateChangeType::DataBuffersReturned: OnDataBuffersReturned(change); break;
			//case PartyStateChangeType::EndpointPropertiesChanged: OnEndpointPropertiesChanged(change); break;
			//case PartyStateChangeType::SynchronizeMessagesBetweenEndpointsCompleted: OnSynchronizeMessagesBetweenEndpointsCompleted(change); break;
			//case PartyStateChangeType::CreateInvitationCompleted: OnCreateInvitationCompleted(change); break;
			//case PartyStateChangeType::RevokeInvitationCompleted: OnRevokeInvitationCompleted(change); break;
			//case PartyStateChangeType::InvitationCreated: OnInvitationCreated(change); break;
			//case PartyStateChangeType::InvitationDestroyed: OnInvitationDestroyed(change); break;
			//case PartyStateChangeType::NetworkPropertiesChanged: OnNetworkPropertiesChanged(change); break;
			//case PartyStateChangeType::KickDeviceCompleted: OnKickDeviceCompleted(change); break;
			//case PartyStateChangeType::KickUserCompleted: OnKickUserCompleted(change); break;
			case PartyStateChangeType::CreateChatControlCompleted: OnCreateChatControlCompleted(change); break;
			//case PartyStateChangeType::DestroyChatControlCompleted: OnDestroyChatControlCompleted(change); break;
			//case PartyStateChangeType::ChatControlCreated: OnChatControlCreated(change); break;
			//case PartyStateChangeType::ChatControlDestroyed: OnChatControlDestroyed(change); break;
			//case PartyStateChangeType::SetChatAudioEncoderBitrateCompleted: OnSetChatAudioEncoderBitrateCompleted(change); break;
			//case PartyStateChangeType::ChatTextReceived: OnChatTextReceived(change); break;
			//case PartyStateChangeType::VoiceChatTranscriptionReceived: OnVoiceChatTranscriptionReceived(change); break;
			case PartyStateChangeType::SetChatAudioInputCompleted: OnSetChatAudioInputCompleted(change); break;
			case PartyStateChangeType::SetChatAudioOutputCompleted: OnSetChatAudioOutputCompleted(change); break;
			//case PartyStateChangeType::LocalChatAudioInputChanged: OnLocalChatAudioInputChanged(change); break;
			//case PartyStateChangeType::LocalChatAudioOutputChanged: OnLocalChatAudioOutputChanged(change); break;
			//case PartyStateChangeType::SetTextToSpeechProfileCompleted: OnSetTextToSpeechProfileCompleted(change); break;
			//case PartyStateChangeType::SynthesizeTextToSpeechCompleted: OnSynthesizeTextToSpeechCompleted(change); break;
			//case PartyStateChangeType::SetLanguageCompleted: OnSetLanguageCompleted(change); break;
			//case PartyStateChangeType::SetTranscriptionOptionsCompleted: OnSetTranscriptionOptionsCompleted(change); break;
			//case PartyStateChangeType::SetTextChatOptionsCompleted: OnSetTextChatOptionsCompleted(change); break;
			//case PartyStateChangeType::ChatControlPropertiesChanged: OnChatControlPropertiesChanged(change); break;
			//case PartyStateChangeType::ChatControlJoinedNetwork: OnChatControlJoinedNetwork(change); break;
			//case PartyStateChangeType::ChatControlLeftNetwork: OnChatControlLeftNetwork(change); break;
			case PartyStateChangeType::ConnectChatControlCompleted: OnConnectChatControlCompleted(change); break;
			//case PartyStateChangeType::DisconnectChatControlCompleted: OnDisconnectChatControlCompleted(change); break;
			//case PartyStateChangeType::PopulateAvailableTextToSpeechProfilesCompleted: OnPopulateAvailableTextToSpeechProfilesCompleted(change); break;
			//case PartyStateChangeType::ConfigureAudioManipulationVoiceStreamCompleted: OnConfigureAudioManipulationVoiceStreamCompleted(change); break;
			//case PartyStateChangeType::ConfigureAudioManipulationCaptureStreamCompleted: OnConfigureAudioManipulationCaptureStreamCompleted(change); break;
			//case PartyStateChangeType::ConfigureAudioManipulationRenderStreamCompleted: OnConfigureAudioManipulationRenderStreamCompleted(change); break;
			}
		}
	}

	// Return the processed changes back to the PartyManager
	err = PartyManager::GetSingleton().FinishProcessingStateChanges(count, changes);

	if (PARTY_FAILED(err))
	{
		PARTY_DBG_TRACE("PlayFabPartyManager::Process() - PartyManager::FinishProcessingStateChanges failed: %hs\n", GetErrorMessage(err));
	}
}

void PlayFabPartyManager::OnCreateLocalChatUserCompleted(const PartyXblStateChange* change)
{
	const PartyXblCreateLocalChatUserCompletedStateChange* result = static_cast<const PartyXblCreateLocalChatUserCompletedStateChange*>(change);
	if (result)
	{
		SetupLocalUserPayload* payload = (SetupLocalUserPayload*)(result->asyncIdentifier);

		XUMuser* xumuser = XUM::GetUserFromId(payload->user_id);
		if ((xumuser != NULL) && (xumuser->playfabState != XUMuserPlayFabState::logging_in))
		{
			PARTY_DBG_TRACE("PlayFabPartyManager::OnCreateLocalChatUserCompleted(): User %llu not logging in, so aborting\n", payload->user_id);
			payload->Free("PlayFabPartyManager::OnCreateLocalChatUserCompleted()");

			delete payload;

			return;
		}

		if (result->result == PartyXblStateChangeResult::Succeeded)
		{
			PARTY_DBG_TRACE("PlayFabPartyManager::OnCreateLocalChatUserCompleted(): CreateLocalChatUser completed for user %llu\n", payload->user_id);		

			// Now log in
			PartyError err = PartyXblManager::GetSingleton().LoginToPlayFab(payload->playfabLocalChatUser, payload);
			if (PARTY_FAILED(err))
			{
				PARTY_DBG_TRACE("PlayFabPartyManager::OnCreateLocalChatUserCompleted(): LoginToPlayFab failed: %s\n", GetXblErrorMessage(err));				
				if (xumuser != NULL)
				{
					xumuser->playfabState = XUMuserPlayFabState::logged_out;
				}

				payload->Free("PlayFabPartyManager::OnCreateLocalChatUserCompleted()");

				delete payload;
				
				return;
			}
		}
		else
		{						
			PARTY_DBG_TRACE("PlayFabPartyManager::OnCreateLocalChatUserCompleted(): failed with error %s for user %llu\n", GetXblErrorMessage(result->errorDetail), payload->user_id);
			if (xumuser != NULL)
			{
				xumuser->playfabState = XUMuserPlayFabState::logged_out;
			}

			delete payload;
		}
	}
}

void PlayFabPartyManager::OnLoginToPlayFabCompleted(const PartyXblStateChange* change)
{
	const PartyXblLoginToPlayFabCompletedStateChange* result = static_cast<const PartyXblLoginToPlayFabCompletedStateChange*>(change);
	if (result)
	{
		SetupLocalUserPayload* payload = (SetupLocalUserPayload*)(result->asyncIdentifier);

		XUMuser* xumuser = XUM::GetUserFromId(payload->user_id);
		if ((xumuser != NULL) && (xumuser->playfabState != XUMuserPlayFabState::logging_in))
		{
			PARTY_DBG_TRACE("PlayFabPartyManager::OnLoginToPlayFabCompleted(): User %llu not logging in, so aborting\n", payload->user_id);
			payload->Free("PlayFabPartyManager::OnLoginToPlayFabCompleted()");

			delete payload;

			return;
		}

		if (result->result == PartyXblStateChangeResult::Succeeded)
		{
			PARTY_DBG_TRACE("PlayFabPartyManager::OnLoginToPlayFabCompleted(): LoginToPlayFab completed for user %llu\n", payload->user_id);
			payload->playfabTokenExpiryTime = result->expirationTime;

			XUMuser* xumuser = XUM::GetUserFromId(payload->user_id);
			if ((xumuser != NULL) && (xumuser->playfabLocalUser != NULL))
			{
				xumuser->playfabTokenExpiryTime = result->expirationTime;	// write this in immediately if we are refreshing an existing login
				xumuser->playfabState = XUMuserPlayFabState::logged_in;

				payload->Free("PlayFabPartyManager::OnLoginToPlayFabCompleted()");

				delete payload;
			}
			else if ((xumuser != NULL) && (xumuser->playfabLocalUser == NULL))
			{				
				// Now create local user
				PartyError err = PartyManager::GetSingleton().CreateLocalUser(
					result->entityId,                    // User id
					result->titlePlayerEntityToken,                 // User entity token
					&(payload->playfabLocalUser)                // OUT local user object
				);

				if (PARTY_FAILED(err))
				{
					PARTY_DBG_TRACE("PlayFabPartyManager::OnLoginToPlayFabCompleted(): CreateLocalUser() failed with error %s for user %llu\n", GetXblErrorMessage(err), payload->user_id);
					xumuser->playfabState = XUMuserPlayFabState::logged_out;

					payload->Free("PlayFabPartyManager::OnLoginToPlayFabCompleted()");

					delete payload;
				}
				else
				{
					PARTY_DBG_TRACE("PlayFabPartyManager::OnLoginToPlayFabCompleted(): CreateLocalUser succeeded for user %llu\n", payload->user_id);

					// Now create chat control
					PartyLocalDevice* localDevice = NULL;
					err = PartyManager::GetSingleton().GetLocalDevice(&localDevice);

					if (PARTY_FAILED(err))
					{
						PARTY_DBG_TRACE("PlayFabPartyManager::OnLoginToPlayFabCompleted(): GetLocalDevice() failed with error %s for user %llu\n", GetErrorMessage(err), payload->user_id);

						payload->Free("PlayFabPartyManager::OnLoginToPlayFabCompleted()");

						xumuser->playfabState = XUMuserPlayFabState::logged_out;

						delete payload;
					}
					else
					{
						err = localDevice->CreateChatControl(
							payload->playfabLocalUser,
							NULL,
							payload,
							&(payload->playfabLocalChatControl)
						);

						if (PARTY_FAILED(err))
						{
							PARTY_DBG_TRACE("PlayFabPartyManager::OnLoginToPlayFabCompleted(): GetLocalDevice() failed with error %s for user %llu\n", GetErrorMessage(err), payload->user_id);

							payload->Free("PlayFabPartyManager::OnLoginToPlayFabCompleted()");

							xumuser->playfabState = XUMuserPlayFabState::logged_out;

							delete payload;
						}

					}					
				}				
			}
			else
			{
				payload->Free("PlayFabPartyManager::OnLoginToPlayFabCompleted()");				

				delete payload;
			}

		}
		else
		{
			XUMuser* xumuser = XUM::GetUserFromId(payload->user_id);

			PARTY_DBG_TRACE("PlayFabPartyManager::OnLoginToPlayFabCompleted(): failed with error %s for user %llu\n", GetXblErrorMessage(result->errorDetail), payload->user_id);			

			if (xumuser != NULL)
			{
				xumuser->playfabState = XUMuserPlayFabState::logged_out;
			}

			payload->Free("PlayFabPartyManager::OnLoginToPlayFabCompleted()");

			delete payload;
		}
	}
}

void PlayFabPartyManager::OnRequiredChatPermissionInfoChanged(const Party::PartyXblStateChange* change)
{
	const PartyXblRequiredChatPermissionInfoChangedStateChange* result = static_cast<const PartyXblRequiredChatPermissionInfoChangedStateChange*>(change);
	if (result)
	{
		PartyXblLocalChatUser* localChatUser = result->localChatUser;
		PartyXblChatUser* targetChatUser = result->targetChatUser;

		uint64_t localuserID = 0;
		PartyError error = localChatUser->GetXboxUserId(&localuserID);
		if (PARTY_FAILED(error))
		{
			PARTY_DBG_TRACE("PlayFabPartyManager::OnRequiredChatPermissionInfoChanged(): GetXboxUserId() failed with error %s\n", GetXblErrorMessage(error));
			return;
		}

		uint64_t targetuserID = 0;
		error = targetChatUser->GetXboxUserId(&targetuserID);
		if (PARTY_FAILED(error))
		{
			PARTY_DBG_TRACE("PlayFabPartyManager::OnRequiredChatPermissionInfoChanged(): GetXboxUserId() failed with error %s\n", GetXblErrorMessage(error));
			return;
		}

		XUMuser* xumuser = XUM::GetUserFromId(localuserID);
		if (xumuser == NULL)
		{
			PARTY_DBG_TRACE("PlayFabPartyManager::OnRequiredChatPermissionInfoChanged(): Couldn't find local user %lld\n", localuserID);
			return;
		}

		uint64_t permissions = xumuser->GetUserChatPermissions(targetuserID);
		SetChatPermissions(localuserID, targetuserID, permissions);		// reset permissions for this user
	}
}

void PlayFabPartyManager::OnCreateNewNetworkCompleted(const Party::PartyStateChange* change)
{	
	const PartyCreateNewNetworkCompletedStateChange* result = static_cast<const PartyCreateNewNetworkCompletedStateChange*>(change);
	if (result)
	{
		XSM::OnPlayFabPartyChange(change);

		PlayFabPayloadBase* payload = (PlayFabPayloadBase*)(result->asyncIdentifier);
		delete payload;
	}
}

void PlayFabPartyManager::OnConnectToNetworkCompleted(const Party::PartyStateChange* change)
{
	const PartyConnectToNetworkCompletedStateChange* result = static_cast<const PartyConnectToNetworkCompletedStateChange*>(change);
	if (result)
	{
		XSM::OnPlayFabPartyChange(change);

		PlayFabPayloadBase* payload = (PlayFabPayloadBase*)(result->asyncIdentifier);
		delete payload;
	}
}

void PlayFabPartyManager::OnAuthenticateLocalUserCompleted(const Party::PartyStateChange* change)
{
	const PartyAuthenticateLocalUserCompletedStateChange* result = static_cast<const PartyAuthenticateLocalUserCompletedStateChange*>(change);
	if (result)
	{
		XSM::OnPlayFabPartyChange(change);

		PlayFabPayloadBase* payload = (PlayFabPayloadBase*)(result->asyncIdentifier);
		delete payload;
	}
}

void PlayFabPartyManager::OnConnectChatControlCompleted(const Party::PartyStateChange* change)
{
	const PartyConnectChatControlCompletedStateChange* result = static_cast<const PartyConnectChatControlCompletedStateChange*>(change);
	if (result)
	{
		XSM::OnPlayFabPartyChange(change);

		PlayFabPayloadBase* payload = (PlayFabPayloadBase*)(result->asyncIdentifier);
		delete payload;
	}
}

void PlayFabPartyManager::OnCreateEndpointCompleted(const Party::PartyStateChange* change)
{
	const PartyCreateEndpointCompletedStateChange* result = static_cast<const PartyCreateEndpointCompletedStateChange*>(change);
	if (result)
	{
		XSM::OnPlayFabPartyChange(change);

		PlayFabPayloadBase* payload = (PlayFabPayloadBase*)(result->asyncIdentifier);
		delete payload;
	}
}

void PlayFabPartyManager::AddToNetworkUsers(const char* _networkID, const char* _sendingEntityID, uint64_t _sendingXuid)
{
	auto iNetwork = networkUsers.find(_networkID);
	NetworkUserInfo* pNetwork = (iNetwork != networkUsers.end() ? iNetwork->second.get() : NULL);

	if (pNetwork == NULL)
	{
		NetworkUserInfo* newNetwork = new NetworkUserInfo();
		strcpy(newNetwork->networkIdentifier, _networkID);

		networkUsers[_networkID] = std::unique_ptr<NetworkUserInfo>(newNetwork);

		pNetwork = newNetwork;
	}
	
	pNetwork->users[_sendingEntityID] = _sendingXuid;
}

void PlayFabPartyManager::RemoveFromNetworkUsers(const char* _networkID, const char* _sendingEntityID)
{
	auto iNetwork = networkUsers.find(_networkID);
	if(iNetwork == networkUsers.end() || iNetwork->second == NULL)
		return;		// network doesn't exist

	NetworkUserInfo* pNetwork = iNetwork->second.get();

	pNetwork->users.erase(_sendingEntityID);

	if (pNetwork->users.empty())
	{
		// Delete network
		networkUsers.erase(iNetwork);
	}
}

void PlayFabPartyManager::RemoveFromNetworkUsers(const char* _networkID, uint64_t _sendingXuid)
{
	auto iNetwork = networkUsers.find(_networkID);
	if (iNetwork == networkUsers.end() || iNetwork->second == NULL)
		return;		// network doesn't exist

	NetworkUserInfo* pNetwork = iNetwork->second.get();

	auto iUserToErase = std::find_if(pNetwork->users.begin(), pNetwork->users.end(),
		[&](const std::pair<std::string, uint64_t>& e) { return e.second == _sendingXuid; });

	if(iUserToErase == pNetwork->users.end())
		return;		// not found

	pNetwork->users.erase(iUserToErase);

	if (pNetwork->users.empty())
	{
		// Delete network
		networkUsers.erase(iNetwork);
	}
}

bool PlayFabPartyManager::ExistsInNetworkUsers(const char* _networkID, const char* _entityID)
{
	auto iNetwork = networkUsers.find(_networkID);
	if(iNetwork == networkUsers.end() || iNetwork->second == NULL)
		return false;		// network doesn't exist

	NetworkUserInfo* pNetwork = iNetwork->second.get();
	auto iUser = pNetwork->users.find(_entityID);
	if (iUser != pNetwork->users.end())
		return true;
	
	return false;
}

bool PlayFabPartyManager::ExistsInNetworkUsers(const char* _networkID, uint64_t _user_id)
{
	auto iNetwork = networkUsers.find(_networkID);
	if(iNetwork == networkUsers.end() || iNetwork->second == NULL)
		return false;		// network doesn't exist

	NetworkUserInfo* pNetwork = iNetwork->second.get();

	// Need to do this the hard way	
	for (auto iUser = pNetwork->users.begin(); iUser != pNetwork->users.end(); ++iUser)
	{
		if (iUser->second == _user_id)
		{
			return true;
		}
	}

	return false;
}

void PlayFabPartyManager::RemoveBufferedPacketsForUser(const char* _networkID, const char* _sendingEntityID)
{
	auto iNetwork = packetBuffer.find(_networkID);
	if (iNetwork == packetBuffer.end() || iNetwork->second == NULL)
		return;

	iNetwork->second->users.erase(_sendingEntityID);	
}

void PlayFabPartyManager::RemoveBufferedPacketsForUser(const char* _networkID, uint64_t _sendingXuid)
{
	// Since incoming packets only have PlayFab entity IDs and not XUIDs we don't categorise them that way
	// However, we can try to find which entity ID corresponds to the entity ID by looking it up in the networkUsers table (if it exists in there)
	auto iNetwork = networkUsers.find(_networkID);
	if(iNetwork == networkUsers.end() || iNetwork->second == NULL)
		return;		// network doesn't exist

	NetworkUserInfo* pNetwork = iNetwork->second.get();

	// Need to do this the hard way
	for (auto iUser = pNetwork->users.begin(); iUser != pNetwork->users.end(); ++iUser)
	{
		if (iUser->second == _sendingXuid)
		{
			RemoveBufferedPacketsForUser(_networkID, iUser->first.c_str());
			break;
		}
	}
}

void PlayFabPartyManager::RemoveBufferedPacketsForNetwork(const char* _networkID)
{
	packetBuffer.erase(_networkID);
}

bool PlayFabPartyManager::ShouldBuffer(const char* _networkID, const char* _sendingEntityID)
{
	auto iUserInfo = networkUsers.find(_networkID);
	if(iUserInfo == networkUsers.end() || iUserInfo->second == NULL)
		return true;

	NetworkUserInfo* pUserInfo = iUserInfo->second.get();
	auto iUser = pUserInfo->users.find(_sendingEntityID);
	if (iUser != pUserInfo->users.end())
		return true;

	return false;
}

void PlayFabPartyManager::BufferPacket(const char* _networkID, const char* _sendingEntityID, const char* _receivingEntityID, const void* _messageBuffer, uint32_t _messageSize)
{
	auto i = packetBuffer.find(_networkID);
	PacketBufferNetwork* pNetwork = (i != packetBuffer.end() ? i->second.get() : NULL);

	if (pNetwork == NULL)
	{
		PacketBufferNetwork* newNetwork = new PacketBufferNetwork();
		strcpy(newNetwork->networkIdentifier, _networkID);

		packetBuffer[_networkID] = std::unique_ptr<PacketBufferNetwork>(newNetwork);

		pNetwork = newNetwork;
	}
	pNetwork->AddPacket(_sendingEntityID, _receivingEntityID, _messageBuffer, _messageSize);
}

void PlayFabPartyManager::FlushBufferedPackets()
{
	for (auto iNetwork = packetBuffer.begin(); iNetwork != packetBuffer.end(); ++iNetwork)
	{
		PacketBufferNetwork* pNetwork = iNetwork->second.get();

		if (pNetwork != NULL)
		{
			auto iUserInfo = networkUsers.find(iNetwork->first);
			if(iUserInfo != networkUsers.end() && iUserInfo->second.get() != NULL)
			{
				pNetwork->DispatchPackets(iUserInfo->second.get());

				if (pNetwork->users.empty())
				{
					iNetwork = packetBuffer.erase(iNetwork);
					continue;
				}
			}
		}

		++iNetwork;
	}
}

void PlayFabPartyManager::OnEndpointMessageReceived(const Party::PartyStateChange* change)
{
	const PartyEndpointMessageReceivedStateChange* result = static_cast<const PartyEndpointMessageReceivedStateChange*>(change);
	if (result)
	{
		if (result->network == NULL)
		{
			PARTY_DBG_TRACE("PlayFabPartyManager::OnEndpointMessageReceived(): incoming message has no network information\n");
			return;
		}

		PartyEndpoint* endpoint = result->senderEndpoint;
		if (endpoint == NULL)
		{
			PARTY_DBG_TRACE("PlayFabPartyManager::OnEndpointMessageReceived(): incoming message has corrupted endpoint\n");
			return;
		}

		const char* entityID = NULL;
		PartyError error = endpoint->GetEntityId(&entityID);
		if (PARTY_FAILED(error))
		{
			PARTY_DBG_TRACE("PlayFabPartyManager::OnEndpointMessageReceived(): GetEntityId() failed with error %s\n", GetErrorMessage(error));
			return;
		}

		PartyNetworkDescriptor networkDescriptor;
		error = result->network->GetNetworkDescriptor(&networkDescriptor);
		if (PARTY_FAILED(error))
		{
			PARTY_DBG_TRACE("PlayFabPartyManager::OnEndpointMessageReceived(): GetNetworkDescriptor() failed with error %s\n", GetErrorMessage(error));
			return;
		}

		bool shouldbuffer = ShouldBuffer(networkDescriptor.networkIdentifier, entityID);

		for (uint32_t i = 0; i < result->receiverEndpointCount; i++)
		{
			if (result->receiverEndpoints[i] == NULL)
				continue;

			const char* receivingEntityID = NULL;
			error = result->receiverEndpoints[i]->GetEntityId(&receivingEntityID);
			if (PARTY_FAILED(error))
			{
				PARTY_DBG_TRACE("PlayFabPartyManager::OnEndpointMessageReceived(): GetEntityId() failed with error %s\n", GetErrorMessage(error));
				return;
			}			

			if (shouldbuffer)
			{
				BufferPacket(networkDescriptor.networkIdentifier, entityID, receivingEntityID, result->messageBuffer, result->messageSize);
			}
			else
			{
				// FIXME

#if 0
				uint64_t receivingUserID = GetUserFromEntityID(receivingEntityID);

				// Trigger an async event for each endpoint			
				int bufferindex = CreateBuffer(result->messageSize, eBuffer_Format_Fixed, 1);
				IBuffer* pBuff = GetIBuffer(bufferindex);
				memcpy(pBuff->GetBuffer(), result->messageBuffer, result->messageSize);

				int dsMapIndex = GDKX::CreateDsMap(0);

				GDKX::DsMapAddDouble(dsMapIndex, "type", eNetworkingEventType_IncomingData);
				GDKX::DsMapAddString(dsMapIndex, "id", entityID);			// Server that's throwing the event
				GDKX::DsMapAddDouble(dsMapIndex, "buffer", bufferindex);
				GDKX::DsMapAddDouble(dsMapIndex, "size", result->messageSize);

				GDKX::DsMapAddString(dsMapIndex, "ip", entityID);		// connecting socket
				GDKX::DsMapAddDouble(dsMapIndex, "port", 0.0);			// connecting socket

				GDKX::DsMapAddString(dsMapIndex, "receivingID", receivingEntityID);		// receiving endpoint
				GDKX::DsMapAddDouble(dsMapIndex, "receivingUser", receivingUserID);		// receiving endpoint

				CreateAsynEventWithDSMapAndBuffer(dsMapIndex, bufferindex, EVENT_OTHER_WEB_NETWORKING);
#endif
			}
		}
	}
}

void PlayFabPartyManager::OnCreateChatControlCompleted(const Party::PartyStateChange* change)
{
	const PartyCreateChatControlCompletedStateChange* result = static_cast<const PartyCreateChatControlCompletedStateChange*>(change);
	if (result)
	{
		SetupLocalUserPayload* payload = (SetupLocalUserPayload*)(result->asyncIdentifier);
		XUMuser* xumuser = XUM::GetUserFromId(payload->user_id);		
		if ((xumuser != NULL) && (xumuser->playfabState != XUMuserPlayFabState::logging_in))
		{
			PARTY_DBG_TRACE("PlayFabPartyManager::OnCreateChatControlCompleted(): User %llu not logging in, so aborting\n", payload->user_id);
			payload->Free("PlayFabPartyManager::OnCreateChatControlCompleted()");

			delete payload;

			return;
		}

		if (result->result == PartyStateChangeResult::Succeeded)
		{			
			PARTY_DBG_TRACE("PlayFabPartyManager::OnCreateChatControlCompleted(): CreateChatControl completed for user %llu\n", payload->user_id);

			std::string xuidString = std::to_string(payload->user_id);

			PartyError err = payload->playfabLocalChatControl->SetAudioInput(
				PartyAudioDeviceSelectionType::PlatformUserDefault,
				xuidString.c_str(),
				payload
			);

			if (PARTY_FAILED(err))
			{
				PARTY_DBG_TRACE("PlayFabPartyManager::OnCreateChatControlCompleted(): SetAudioInput() failed with error %s for user %llu\n", GetErrorMessage(result->errorDetail), payload->user_id);
				if (xumuser != NULL)
				{
					xumuser->playfabState = XUMuserPlayFabState::logged_out;
				}

				payload->Free("PlayFabPartyManager::OnCreateChatControlCompleted()");				

				delete payload;
			}
		}
		else
		{			
			PARTY_DBG_TRACE("PlayFabPartyManager::OnCreateChatControlCompleted(): failed with error %s for user %llu\n", GetErrorMessage(result->errorDetail), payload->user_id);
			if (xumuser != NULL)
			{
				xumuser->playfabState = XUMuserPlayFabState::logged_out;
			}
			
			payload->Free("PlayFabPartyManager::OnCreateChatControlCompleted()");

			delete payload;
		}
	}
}

void PlayFabPartyManager::OnSetChatAudioInputCompleted(const Party::PartyStateChange* change)
{
	const PartySetChatAudioInputCompletedStateChange* result = static_cast<const PartySetChatAudioInputCompletedStateChange*>(change);
	if (result)
	{
		PlayFabPayloadBase* payloadbase = (PlayFabPayloadBase*)(result->asyncIdentifier);
		if (IsPayloadOfType(payloadbase, PlayFabPayloadType::SetupLocalUser))
		{
			SetupLocalUserPayload* payload = (SetupLocalUserPayload*)(result->asyncIdentifier);
			XUMuser* xumuser = XUM::GetUserFromId(payload->user_id);			
			if ((xumuser != NULL) && (xumuser->playfabState != XUMuserPlayFabState::logging_in))
			{
				PARTY_DBG_TRACE("PlayFabPartyManager::OnSetChatAudioInputCompleted(): User %llu not logging in, so aborting\n", payload->user_id);
				payload->Free("PlayFabPartyManager::OnSetChatAudioInputCompleted()");

				delete payload;

				return;
			}

			if (result->result == PartyStateChangeResult::Succeeded)
			{
				PARTY_DBG_TRACE("PlayFabPartyManager::OnSetChatAudioInputCompleted(): SetAudioInput completed for user %llu\n", payload->user_id);

				std::string xuidString = std::to_string(payload->user_id);

				PartyError err = payload->playfabLocalChatControl->SetAudioOutput(
					PartyAudioDeviceSelectionType::PlatformUserDefault,
					xuidString.c_str(),
					payload
				);

				if (PARTY_FAILED(err))
				{
					PARTY_DBG_TRACE("PlayFabPartyManager::OnSetChatAudioInputCompleted(): SetAudioInput() failed with error %s for user %llu\n", GetErrorMessage(result->errorDetail), payload->user_id);
					if (xumuser != NULL)
					{
						xumuser->playfabState = XUMuserPlayFabState::logged_out;
					}

					payload->Free("PlayFabPartyManager::OnSetChatAudioInputCompleted()");

					delete payload;
				}				
			}
			else
			{
				PARTY_DBG_TRACE("PlayFabPartyManager::OnSetChatAudioInputCompleted(): SetAudioInput() failed with error %s for user %llu\n", GetErrorMessage(result->errorDetail), payload->user_id);
				if (xumuser != NULL)
				{
					xumuser->playfabState = XUMuserPlayFabState::logged_out;
				}

				payload->Free("PlayFabPartyManager::OnSetChatAudioInputCompleted()");

				delete payload;
			}
		}
		else
		{
		}
	}
}

void PlayFabPartyManager::OnSetChatAudioOutputCompleted(const Party::PartyStateChange* change)
{
	const PartySetChatAudioOutputCompletedStateChange* result = static_cast<const PartySetChatAudioOutputCompletedStateChange*>(change);
	if (result)
	{
		PlayFabPayloadBase* payloadbase = (PlayFabPayloadBase*)(result->asyncIdentifier);
		if (IsPayloadOfType(payloadbase, PlayFabPayloadType::SetupLocalUser))
		{
			SetupLocalUserPayload* payload = (SetupLocalUserPayload*)(result->asyncIdentifier);
			XUMuser* xumuser = XUM::GetUserFromId(payload->user_id);			
			if ((xumuser != NULL) && (xumuser->playfabState != XUMuserPlayFabState::logging_in))
			{
				PARTY_DBG_TRACE("PlayFabPartyManager::OnSetChatAudioOutputCompleted(): User %llu not logging in, so aborting\n", payload->user_id);
				payload->Free("PlayFabPartyManager::OnSetChatAudioOutputCompleted()");

				delete payload;

				return;
			}

			if (result->result == PartyStateChangeResult::Succeeded)
			{
				PARTY_DBG_TRACE("PlayFabPartyManager::OnSetChatAudioOutputCompleted(): SetAudioOutput completed for user %llu\n", payload->user_id);

				PARTY_DBG_TRACE("PlayFabPartyManager::OnSetChatAudioOutputCompleted(): PlayFab Party setup completed for user %llu\n", payload->user_id);

				// Hooray, user setup all done
				xumuser->playfabLocalChatUser = payload->playfabLocalChatUser;
				xumuser->playfabLocalChatControl = payload->playfabLocalChatControl;
				xumuser->playfabLocalUser = payload->playfabLocalUser;
				xumuser->playfabTokenExpiryTime = payload->playfabTokenExpiryTime;
				xumuser->playfabState = XUMuserPlayFabState::logged_in;

				// TODO: trigger an async event informing the user that playfab login has succeeded (also trigger async events in all the places where it can fail)
				delete payload;
			}
			else
			{
				PARTY_DBG_TRACE("PlayFabPartyManager::OnSetChatAudioOutputCompleted(): SetAudioOutput failed with error %s for user %llu\n", GetErrorMessage(result->errorDetail), payload->user_id);
				if (xumuser != NULL)
				{
					xumuser->playfabState = XUMuserPlayFabState::logged_out;
				}

				payload->Free("PlayFabPartyManager::OnSetChatAudioOutputCompleted()");

				delete payload;
			}
		}
		else
		{
		}
	}
}

int PlayFabPartyManager::SetupLocalUser(uint64_t _user_id, int _requestID)
{
	XUMuser* xumuser = XUM::GetUserFromId(_user_id);
	if (xumuser == NULL)
	{
		PARTY_DBG_TRACE("PlayFabPartyManager::SetupLocalUser(): user %llu not found\n", _user_id);
		return -1;
	}

	if (xumuser->playfabState == XUMuserPlayFabState::logging_in)
	{
		PARTY_DBG_TRACE("PlayFabPartyManager::SetupLocalUser(): already in the middle of logging in user %llu\n", _user_id);
		return 1;
	}

	if (xumuser->playfabState == XUMuserPlayFabState::logged_in)
	{
		PARTY_DBG_TRACE("PlayFabPartyManager::SetupLocalUser(): user %llu already logged in\n", _user_id);
		return 2;
	}

	if (xumuser->playfabState != XUMuserPlayFabState::logged_out)
	{
		PARTY_DBG_TRACE("PlayFabPartyManager::SetupLocalUser(): user %llu not in logged-out state (actual state %d)\n", _user_id, xumuser->playfabState);
		return -2;
	}

	if (xumuser->playfabLocalChatUser != NULL)
	{
		PARTY_DBG_TRACE("PlayFabPartyManager::SetupLocalUser(): user %llu already has local chat user\n", _user_id);
		return -3;
	}

	if (xumuser->playfabLocalUser != NULL)
	{
		PARTY_DBG_TRACE("PlayFabPartyManager::SetupLocalUser(): user %llu already has local user\n", _user_id);
		return -4;
	}

	SetupLocalUserPayload* payload = new SetupLocalUserPayload();
	payload->requestID = _requestID;
	payload->user_id = _user_id;

	PartyError err = PartyXblManager::GetSingleton().CreateLocalChatUser(
		payload->user_id,
		payload,
		&(payload->playfabLocalChatUser)
	);

	if (PARTY_FAILED(err))
	{
		PARTY_DBG_TRACE("PlayFabPartyManager::SetupLocalUser(): CreateLocalChatUser() failed with error %s for user %llu\n", GetXblErrorMessage(err), _user_id);

		delete payload;
		return -5;
	}

	xumuser->playfabState = XUMuserPlayFabState::logging_in;

	return 0;
}

bool PlayFabPartyManager::ShouldRefreshLocalUserLogin(uint64_t _user_id)
{
	XUMuser* xumuser = XUM::GetUserFromId(_user_id);
	if (xumuser == NULL)
	{
		PARTY_DBG_TRACE("PlayFabPartyManager::ShouldRefreshLocalUserLogin(): user %llu not found\n", _user_id);
		return false;
	}

	if (xumuser->playfabState != XUMuserPlayFabState::logged_in)		// we only want to check this if we're currently logged in
	{
		//PARTY_DBG_TRACE("PlayFabPartyManager::ShouldRefreshLocalUserLogin(): user is either logged out or in the process of logging in or out %llu\n", _user_id);
		return false;
	}

	// Refresh the token an hour before expiration
	time_t currentTime = std::time(nullptr);
	if ((xumuser->playfabTokenExpiryTime - 3600) > currentTime)
	{
		// Token is not close to expiry, so bail
		return false;
	}

	return true;
}

int PlayFabPartyManager::RefreshLocalUserLogin(uint64_t _user_id, int _requestID)
{
	XUMuser* xumuser = XUM::GetUserFromId(_user_id);
	if (xumuser == NULL)
	{
		PARTY_DBG_TRACE("PlayFabPartyManager::RefreshLocalUserLogin(): user %llu not found\n", _user_id);
		return -1;
	}

	if (xumuser->playfabState == XUMuserPlayFabState::logging_in)
	{
		PARTY_DBG_TRACE("PlayFabPartyManager::RefreshLocalUserLogin(): already in the middle of logging in user %llu\n", _user_id);
		return 1;
	}

	if (xumuser->playfabLocalChatUser == NULL)
	{
		PARTY_DBG_TRACE("PlayFabPartyManager::RefreshLocalUserLogin(): user %llu has no local chat user\n", _user_id);
		return -3;
	}

	SetupLocalUserPayload* payload = new SetupLocalUserPayload();
	payload->requestID = _requestID;
	payload->user_id = _user_id;

	PartyError err = PartyXblManager::GetSingleton().LoginToPlayFab(xumuser->playfabLocalChatUser, payload);
	if (PARTY_FAILED(err))
	{
		PARTY_DBG_TRACE("PlayFabPartyManager::RefreshLocalUserLogin(): LoginToPlayFab failed: %s\n", GetXblErrorMessage(err));
		xumuser->playfabState = XUMuserPlayFabState::logged_out;

		payload->Free("PlayFabPartyManager::RefreshLocalUserLogin()");

		delete payload;
		return -4;
	}

	xumuser->playfabState = XUMuserPlayFabState::logging_in;

	return 0;
}

int PlayFabPartyManager::CleanupLocalUser(uint64_t _user_id)
{
	XUMuser* xumuser = XUM::GetUserFromId(_user_id);
	if (xumuser == NULL)
	{
		PARTY_DBG_TRACE("PlayFabPartyManager::CleanupLocalUser(): user %llu not found\n", _user_id);
		return -1;
	}

	PartyError err;
	if (xumuser->playfabLocalChatControl != NULL)
	{
		PartyLocalDevice* localDevice = NULL;
		err = PartyManager::GetSingleton().GetLocalDevice(&localDevice);
		if (PARTY_FAILED(err))
		{
			PARTY_DBG_TRACE("PlayFabPartyManager::CleanupLocalUser(): GetLocalDevice() failed with error %s for user %llu\n", GetErrorMessage(err), _user_id);
		}
		else
		{
			err = localDevice->DestroyChatControl(xumuser->playfabLocalChatControl, NULL);
			if (PARTY_FAILED(err))
			{
				PARTY_DBG_TRACE("PlayFabPartyManager::CleanupLocalUser(): DestroyChatControl() failed with error %s for user %llu\n", GetErrorMessage(err), _user_id);
			}
			else
			{
				xumuser->playfabLocalChatControl = NULL;
			}
		}
	}

	if (xumuser->playfabLocalUser != NULL)
	{
		err = PartyManager::GetSingleton().DestroyLocalUser(xumuser->playfabLocalUser, NULL);
		if (PARTY_FAILED(err))
		{
			PARTY_DBG_TRACE("PlayFabPartyManager::CleanupLocalUser(): DestroyLocalUser() failed with error %s for user %llu\n", GetErrorMessage(err), _user_id);
		}
		else
		{
			xumuser->playfabLocalUser = NULL;
		}
	}

	if (xumuser->playfabLocalChatUser != NULL)
	{
		err = PartyXblManager::GetSingleton().DestroyChatUser(xumuser->playfabLocalChatUser);
		if (PARTY_FAILED(err))
		{
			PARTY_DBG_TRACE("PlayFabPartyManager::CleanupLocalUser(): DestroyChatUser() failed with error %s for user %llu\n", GetXblErrorMessage(err), _user_id);
		}
		else
		{
			xumuser->playfabLocalChatUser = NULL;
		}
	}

	xumuser->playfabState = XUMuserPlayFabState::logged_out;
	return 0;
}

int PlayFabPartyManager::CreateNetwork(uint64_t _user_id, int _maxPlayers, XSMPlayFabPayload* _payload)
{	
	XUMuser* xumuser = XUM::GetUserFromId(_user_id);
	if (xumuser == NULL)
	{
		PARTY_DBG_TRACE("PlayFabPartyManager::CreateNetwork(): user %llu not found\n", _user_id);
		return -1;
	}

	if (xumuser->playfabState != XUMuserPlayFabState::logged_in)
	{
		PARTY_DBG_TRACE("PlayFabPartyManager::CreateNetwork(): user %llu not logged in to PlayFab Party\n", _user_id);
		return -1;
	}

	if (xumuser->playfabLocalUser == NULL)
	{
		PARTY_DBG_TRACE("PlayFabPartyManager::CreateNetwork(): no PartyLocaluser for user %llu \n", _user_id);
		return -1;
	}

	PartyNetworkConfiguration networkConfiguration = {};
	networkConfiguration.maxUserCount = _maxPlayers;
	networkConfiguration.maxDeviceCount = _maxPlayers;
	networkConfiguration.maxUsersPerDeviceCount = std::min<uint32_t>(_maxPlayers, c_maxLocalUsersPerDeviceCount);		// support hybrid split-screen/online
	networkConfiguration.maxDevicesPerUserCount = 1;														// each user must be on only one device
	networkConfiguration.maxEndpointsPerDeviceCount = networkConfiguration.maxUsersPerDeviceCount;			// max of one endpoint per user on a device

	PartyError error = PartyManager::GetSingleton().CreateNewNetwork(
		xumuser->playfabLocalUser,
		&networkConfiguration,
		0,
		nullptr,
		nullptr,
		_payload,
		nullptr,
		nullptr);
	if (PARTY_FAILED(error))
	{
		PARTY_DBG_TRACE("PlayFabPartyManager::CreateNetwork(): CreateNewNetwork() failed with error %s for user %llu\n", GetErrorMessage(error), _user_id);
		return -1;
	}

	return 0;
}

int PlayFabPartyManager::ConnectToNetwork(PartyNetworkDescriptor* _networkDescriptor, XSMPlayFabPayload* _payload)
{
	if (_networkDescriptor == NULL)
		return -1;

	if (_payload == NULL)
		return -1;

	PartyError error = PartyManager::GetSingleton().ConnectToNetwork(_networkDescriptor, _payload, NULL);
	if (PARTY_FAILED(error))
	{
		PARTY_DBG_TRACE("PlayFabPartyManager::ConnectToNetwork(): ConnectToNetwork() failed with error %s\n", GetErrorMessage(error));
		return -1;
	}

	return 0;
}

int PlayFabPartyManager::AuthenticateLocalUser(Party::PartyNetwork* _network, uint64_t _user_id, char* _invitationID, XSMPlayFabPayload* _payload)
{
	if (_network == NULL)
		return -1;

	if (_user_id == 0)
		return -1;

	if (_invitationID == NULL)
		return -1;

	if (_payload == NULL)
		return -1;

	XUMuser* xumuser = XUM::GetUserFromId(_user_id);
	if (xumuser == NULL)
		return -1;

	if (xumuser->playfabState != XUMuserPlayFabState::logged_in)
	{
		PARTY_DBG_TRACE("PlayFabPartyManager::AuthenticateLocalUser(): user %lld not logged in (actual state %d)\n", _user_id, xumuser->playfabState);
		return -1;
	}

	if (xumuser->playfabLocalUser == NULL)
	{
		PARTY_DBG_TRACE("PlayFabPartyManager::AuthenticateLocalUser(): user %lld not logged in (actual state %d)\n", _user_id, xumuser->playfabState);
		return -1;
	}

	PartyError error = _network->AuthenticateLocalUser(xumuser->playfabLocalUser, _invitationID, _payload);
	if (PARTY_FAILED(error))
	{
		PARTY_DBG_TRACE("PlayFabPartyManager::AuthenticateLocalUser(): AuthenticateLocalUser() failed with error %s\n", GetErrorMessage(error));
		return -1;
	}

	return 0;
}

int PlayFabPartyManager::ConnectChatControl(Party::PartyNetwork* _network, uint64_t _user_id, XSMPlayFabPayload* _payload)
{
	if (_network == NULL)
		return -1;

	if (_user_id == 0)
		return -1;

	if (_payload == NULL)
		return -1;

	XUMuser* xumuser = XUM::GetUserFromId(_user_id);
	if (xumuser == NULL)
		return -1;

	if (xumuser->playfabState != XUMuserPlayFabState::logged_in)
	{
		PARTY_DBG_TRACE("PlayFabPartyManager::ConnectChatControl(): user %lld not logged in (actual state %d)\n", _user_id, xumuser->playfabState);
		return -1;
	}

	if (xumuser->playfabLocalChatControl == NULL)
	{
		PARTY_DBG_TRACE("PlayFabPartyManager::ConnectChatControl(): user %lld has no local chat control\n", _user_id);
		return -1;
	}

	PartyError error = _network->ConnectChatControl(xumuser->playfabLocalChatControl, _payload);
	if (PARTY_FAILED(error))
	{
		PARTY_DBG_TRACE("PlayFabPartyManager::ConnectChatControl(): ConnectChatControl() failed with error %s\n", GetErrorMessage(error));
		return -1;
	}

	return 0;
}

int PlayFabPartyManager::CreateEndpoint(Party::PartyNetwork* _network, uint64_t _user_id, XSMPlayFabPayload* _payload)
{
	if (_network == NULL)
		return -1;

	if (_user_id == 0)
		return -1;

	if (_payload == NULL)
		return -1;

	XUMuser* xumuser = XUM::GetUserFromId(_user_id);
	if (xumuser == NULL)
		return -1;

	if (xumuser->playfabState != XUMuserPlayFabState::logged_in)
	{
		PARTY_DBG_TRACE("PlayFabPartyManager::CreateEndpoint(): user %lld not logged in (actual state %d)\n", _user_id, xumuser->playfabState);
		return -1;
	}

	if (xumuser->playfabLocalUser == NULL)
	{
		PARTY_DBG_TRACE("PlayFabPartyManager::CreateEndpoint(): user %lld has no local user object\n", _user_id);
		return -1;
	}

	PartyError error = _network->CreateEndpoint(xumuser->playfabLocalUser, 0, NULL, NULL, _payload, NULL);
	if (PARTY_FAILED(error))
	{
		PARTY_DBG_TRACE("PlayFabPartyManager::CreateEndpoint(): CreateEndpoint() failed with error %s\n", GetErrorMessage(error));
		return -1;
	}

	return 0;
}

int PlayFabPartyManager::SerialiseNetworkDescriptor(Party::PartyNetworkDescriptor* _networkDescriptor, char* _output)
{
	if (_networkDescriptor == NULL)
		return -1;

	if (_output == NULL)
		return -1;

	PartyError error = PartyManager::GetSingleton().SerializeNetworkDescriptor(_networkDescriptor, _output);
	if (PARTY_FAILED(error))
	{
		PARTY_DBG_TRACE("PlayFabPartyManager::SerialiseNetworkDescriptor(): SerializeNetworkDescriptor() failed with error %s\n", GetErrorMessage(error));
		return -1;
	}

	return 0;
}

int PlayFabPartyManager::DeserialiseNetworkDescriptor(const char* _input, Party::PartyNetworkDescriptor* _networkDescriptor)
{
	if (_input == NULL)
		return -1;

	if (_networkDescriptor == NULL)
		return -1;

	PartyError error = PartyManager::GetSingleton().DeserializeNetworkDescriptor(_input, _networkDescriptor);
	if (PARTY_FAILED(error))
	{
		PARTY_DBG_TRACE("PlayFabPartyManager::DeserialiseNetworkDescriptor(): DeserializeNetworkDescriptor() failed with error %s\n", GetErrorMessage(error));
		return -1;
	}

	return 0;
}

const char* PlayFabPartyManager::GetUserEntityID(uint64_t _user_id)
{
	XUMuser* xumuser = XUM::GetUserFromId(_user_id);
	if (xumuser == NULL)
	{
		PARTY_DBG_TRACE("PlayFabPartyManager::GetUserEntityID(): user %lld not found\n", _user_id);
		return NULL;
	}

	if (xumuser->playfabLocalUser == NULL)
	{
		PARTY_DBG_TRACE("PlayFabPartyManager::GetUserEntityID(): user %lld doesn't have playfab user object\n", _user_id);
		return NULL;
	}

	const char* entityID = NULL;
	PartyError error = xumuser->playfabLocalUser->GetEntityId(&entityID);
	if (PARTY_FAILED(error))
	{
		PARTY_DBG_TRACE("PlayFabPartyManager::GetUserEntityID(): GetEntityId() failed with error %s\n", GetErrorMessage(error));
		return NULL;
	}

	return entityID;
}

uint64_t PlayFabPartyManager::GetUserFromEntityID(const char* _entityID)
{
	int numusers = 0;
	XUMuser** xumusers = XUM::GetUsers(numusers);

	for (int i = 0; i < numusers; i++)
	{
		XUMuser* xumuser = xumusers[i];
		if (xumuser == NULL)
			continue;

		if (xumuser->playfabLocalUser == NULL)
			continue;

		const char* entityID = NULL;
		PartyError error = xumuser->playfabLocalUser->GetEntityId(&entityID);
		if (PARTY_FAILED(error))
		{
			PARTY_DBG_TRACE("PlayFabPartyManager::GetUserFromEntityID(): GetEntityId() failed with error %s\n", GetErrorMessage(error));
			return 0;
		}

		if (strcmp(entityID, _entityID) == 0)
			return xumuser->XboxUserIdInt;
	}

	return 0;
}

int PlayFabPartyManager::SendPacket(uint64_t _user_id, const char* _networkID, const char* _targetEntityID, const void* _pBuff, int _size, bool _reliable)
{
	if (_pBuff == NULL)
	{
		PARTY_DBG_TRACE("PlayFabPartyManager::SendPacket(): data buffer is NULL\n");
		return -1;
	}

	XUMuser* xumuser = XUM::GetUserFromId(_user_id);
	if (xumuser == NULL)
	{
		PARTY_DBG_TRACE("PlayFabPartyManager::SendPacket(): user %llu not found\n", _user_id);
		return -1;
	}

	if (xumuser->playfabState != XUMuserPlayFabState::logged_in)
	{
		PARTY_DBG_TRACE("PlayFabPartyManager::SendPacket(): user %llu not logged in to PlayFab Party\n", _user_id);
		return -1;
	}

	if (xumuser->playfabLocalUser == NULL)
	{
		PARTY_DBG_TRACE("PlayFabPartyManager::SendPacket(): no PartyLocaluser for user %llu \n", _user_id);
		return -1;
	}

	// Get user entity ID
	const char* sendingEntityID = GetUserEntityID(_user_id);
	if (sendingEntityID == NULL)
	{
		// Error reported in GetUserEntityID()
		return -1;
	}

	if ((_targetEntityID != NULL) && ((strcmp(_targetEntityID, "127.0.0.1") == 0) || (strcmp(_targetEntityID, "localhost") == 0) || (strcmp(_targetEntityID, sendingEntityID) == 0)))
	{
		SendLoopbackPacket(sendingEntityID, _targetEntityID, _pBuff, _size);
		return 0;
	}

	bool broadcast = (_targetEntityID == NULL) ? true : false;

	// Get network
	PartyNetwork* network = NULL;
	if (_networkID != NULL)
	{
		network = GetNetworkFromID(_networkID);
	}

	if (network == NULL)
	{
		if (broadcast)
		{
			network = GetNetworkContainingEntity(sendingEntityID);
		}
		else
		{
			network = GetNetworkContainingEntities(sendingEntityID, _targetEntityID);
		}
	}

	if (network == NULL)
	{
		PARTY_DBG_TRACE("PlayFabPartyManager::SendPacket(): couldn't resolve network to send message within\n", _user_id);
		return -1;
	}

	// Now get endpoints
	uint32_t numEndpoints = 0;
	PartyEndpointArray endpoints = NULL;
	PartyError error = network->GetEndpoints(&numEndpoints, &endpoints);
	if (PARTY_FAILED(error))
	{
		PARTY_DBG_TRACE("PlayFabPartyManager::SendPacket(): GetEndpoints() failed with error %s\n", GetErrorMessage(error));
		return -1;
	}

	PartyLocalEndpoint* pSendingEndpoint = NULL;
	PartyEndpoint* pDestEndpoint = NULL;

	for (uint32_t i = 0; i < numEndpoints; i++)
	{
		const char* endpointEntityID = NULL;
		error = endpoints[i]->GetEntityId(&endpointEntityID);
		if (PARTY_FAILED(error))
		{
			PARTY_DBG_TRACE("PlayFabPartyManager::SendPacket(): GetEntityId() failed with error %s\n", GetErrorMessage(error));
			return -1;
		}

		if (endpointEntityID == NULL)
		{
			PARTY_DBG_TRACE("PlayFabPartyManager::SendPacket(): GetEntityId() returned NULL entity ID\n");	
			return -1;
		}

		if (strcmp(sendingEntityID, endpointEntityID) == 0)
		{
			error = endpoints[i]->GetLocal(&pSendingEndpoint);
			if (PARTY_FAILED(error))
			{
				PARTY_DBG_TRACE("PlayFabPartyManager::SendPacket(): GetLocal() failed with error %s\n", GetErrorMessage(error));
				return -1;
			}	

			if (broadcast)
				break;
		}

		if (!broadcast && (strcmp(_targetEntityID, endpointEntityID) == 0))
		{
			pDestEndpoint = endpoints[i];
		}
	}

	if (pSendingEndpoint == NULL)
	{
		PARTY_DBG_TRACE("PlayFabPartyManager::SendPacket(): couldn't find local endpoint with ID %s for use %lld\n", sendingEntityID, _user_id);
		return -1;
	}

	if (!broadcast && (pDestEndpoint == NULL))
	{
		PARTY_DBG_TRACE("PlayFabPartyManager::SendPacket(): couldn't find destination endpoint with ID %s\n", _targetEntityID);
		return -1;
	}

	PartySendMessageOptions sendoptions;
	if (_reliable)
	{
		sendoptions = PartySendMessageOptions::GuaranteedDelivery | PartySendMessageOptions::SequentialDelivery;
	}
	else
	{
		sendoptions = PartySendMessageOptions::Default;
	}

	PartyDataBuffer databuffer;
	databuffer.buffer = _pBuff;
	databuffer.bufferByteCount = _size;

	if (!broadcast)
	{
		error = pSendingEndpoint->SendMessage(1,
			&pDestEndpoint,
			sendoptions,
			NULL,
			1,
			&databuffer,
			NULL
		);
	}
	else
	{
		error = pSendingEndpoint->SendMessage(0,
			NULL,
			sendoptions,
			NULL,
			1,
			&databuffer,
			NULL
		);
	}

	if (PARTY_FAILED(error))
	{
		PARTY_DBG_TRACE("PlayFabPartyManager::SendPacket(): SendMessage() failed with error %s\n", GetErrorMessage(error));
		return -1;
	}

	return 0;
}

int PlayFabPartyManager::SendLoopbackPacket(const char* entityID, const char* _targetEntityID, const void* _pBuff, int _size)
{
	// FIXME

#if 0
	uint64_t user_id = GetUserFromEntityID(entityID);

	// Trigger an async event for each endpoint			
	int bufferindex = CreateBuffer(_size, eBuffer_Format_Fixed, 1);
	IBuffer* pBuff = GetIBuffer(bufferindex);
	memcpy(pBuff->GetBuffer(), _pBuff, _size);

	int dsMapIndex = GDKX::CreateDsMap(0);

	GDKX::DsMapAddDouble(dsMapIndex, "type", eNetworkingEventType_IncomingData);
	GDKX::DsMapAddString(dsMapIndex, "id", _targetEntityID);			// Server that's throwing the event
	GDKX::DsMapAddDouble(dsMapIndex, "buffer", bufferindex);
	GDKX::DsMapAddDouble(dsMapIndex, "size", _size);

	GDKX::DsMapAddString(dsMapIndex, "ip", _targetEntityID);		// connecting socket
	GDKX::DsMapAddDouble(dsMapIndex, "port", 0.0);			// connecting socket

	GDKX::DsMapAddString(dsMapIndex, "receivingID", entityID);		// receiving endpoint
	GDKX::DsMapAddDouble(dsMapIndex, "receivingUser", user_id);		// receiving endpoint

	CreateAsynEventWithDSMapAndBuffer(dsMapIndex, bufferindex, EVENT_OTHER_WEB_NETWORKING);
#endif

	return 0;
}

int PlayFabPartyManager::LeaveNetwork(const char* _networkID)
{
	if (_networkID == NULL)
		return -1;

	PartyNetwork* network = GetNetworkFromID(_networkID);
	if (network != NULL)
	{
		PartyError error = network->LeaveNetwork(NULL);
		if (PARTY_FAILED(error))
		{
			PARTY_DBG_TRACE("PlayFabPartyManager::LeaveNetwork(): LeaveNetwork() failed with error %s\n", GetErrorMessage(error));	
			return -1;
		}
	}
	return 0;
}

int PlayFabPartyManager::MuteRemoteUser(uint64_t _local_user, uint64_t _remote_user, bool _mute)
{
	bool mute_for_all_users = false;
	bool mute_all_remote_users = false;
	const char* localUserEntityID = NULL;
	const char* remoteUserEntityID = NULL;

	if (_local_user == 0)
		mute_for_all_users = true;
	else
	{
		localUserEntityID = XSM::GetSessionUserEntityIDFromID(_local_user);
		if (localUserEntityID == NULL)
		{
			PARTY_DBG_TRACE("PlayFabPartyManager::MuteRemoteUser(): couldn't get local user entity ID\n");
			return -1;
		}
	}

	if (_remote_user == 0)
		mute_all_remote_users = true;
	else
	{
		remoteUserEntityID = XSM::GetSessionUserEntityIDFromID(_remote_user);
		if (remoteUserEntityID == NULL)
		{
			YYFree(localUserEntityID);

			PARTY_DBG_TRACE("PlayFabPartyManager::MuteRemoteUser(): couldn't get remote user entity ID\n");
			return -1;
		}
	}

	// Loop through all the current networks	
	uint32_t numNetworks = 0;
	PartyNetworkArray networks = NULL;
	PartyError error = PartyManager::GetSingleton().GetNetworks(&numNetworks, &networks);
	if (PARTY_FAILED(error))
	{
		PARTY_DBG_TRACE("PlayFabPartyManager::MuteRemoteUser(): GetNetworks() failed with error %s\n", GetErrorMessage(error));		
	}
	else
	{
		for (uint32_t i = 0; i < numNetworks; i++)
		{
			uint32_t numChatControls;
			PartyChatControlArray chatControls;
			error = networks[i]->GetChatControls(&numChatControls, &chatControls);
			if (PARTY_FAILED(error))
			{
				PARTY_DBG_TRACE("PlayFabPartyManager::MuteRemoteUser(): GetChatControls() failed with error %s\n", GetErrorMessage(error));
				continue;
			}

			for (uint32_t j = 0; j < numChatControls; j++)
			{
				PartyLocalChatControl* localChatControl = NULL;
				error = chatControls[j]->GetLocal(&localChatControl);
				if (PARTY_FAILED(error))
				{
					PARTY_DBG_TRACE("PlayFabPartyManager::MuteRemoteUser(): GetLocal() failed with error %s\n", GetErrorMessage(error));
					continue;
				}

				if (localChatControl != NULL)
				{
					const char* entityID = NULL;
					error = localChatControl->GetEntityId(&entityID);
					if (PARTY_FAILED(error))
					{
						PARTY_DBG_TRACE("PlayFabPartyManager::MuteRemoteUser(): GetEntityId() failed with error %s\n", GetErrorMessage(error));
						continue;
					}

					if ((mute_for_all_users) || (strcmp(entityID, localUserEntityID) == 0))
					{
						for (uint32_t k = 0; k < numChatControls; k++)
						{
							if (k == j)
								continue;

							PartyChatControl* remoteChatControl = chatControls[k];		// note that this isn't necessarily remote (could be another local control for a different user)

							const char* otherEntityID = NULL;
							error = localChatControl->GetEntityId(&otherEntityID);
							if (PARTY_FAILED(error))
							{
								PARTY_DBG_TRACE("PlayFabPartyManager::MuteRemoteUser(): GetEntityId() failed with error %s\n", GetErrorMessage(error));
								continue;
							}

							if ((mute_all_remote_users) || (strcmp(otherEntityID, remoteUserEntityID) == 0))
							{
								error = localChatControl->SetIncomingAudioMuted(remoteChatControl, _mute);
								if (PARTY_FAILED(error))
								{
									PARTY_DBG_TRACE("PlayFabPartyManager::MuteRemoteUser(): SetIncomingAudioMuted() failed with error %s for local chat control %s with remote chat control %s\n", GetErrorMessage(error), entityID, otherEntityID);
									continue;
								}
							}							
						}
					}
				}
			}
		}
	}

	YYFree(localUserEntityID);
	YYFree(remoteUserEntityID);

	return 0;
}

bool PlayFabPartyManager::IsRemoteUserMuted(uint64_t _local_user, uint64_t _remote_user)
{
	bool get_for_all_users = false;
	bool get_for_all_remote_users = false;
	const char* localUserEntityID = NULL;
	const char* remoteUserEntityID = NULL;

	if (_local_user == 0)
		get_for_all_users = true;			// we'll return true only if the specified remote user is muted for *all* local users
	else
	{
		localUserEntityID = XSM::GetSessionUserEntityIDFromID(_local_user);
		if (localUserEntityID == NULL)
		{
			PARTY_DBG_TRACE("PlayFabPartyManager::IsRemoteUserMuted(): couldn't get local user entity ID\n");
			return false;
		}
	}

	if (_remote_user == 0)
		get_for_all_remote_users = true;	// we'll return true only if all remote users are muted for the specified local user
	else
	{
		remoteUserEntityID = XSM::GetSessionUserEntityIDFromID(_remote_user);
		if (remoteUserEntityID == NULL)
		{
			YYFree(localUserEntityID);

			PARTY_DBG_TRACE("PlayFabPartyManager::IsRemoteUserMuted(): couldn't get remote user entity ID\n");
			return false;
		}
	}

	bool is_muted = true;

	// Loop through all the current networks	
	uint32_t numNetworks = 0;
	PartyNetworkArray networks = NULL;
	PartyError error = PartyManager::GetSingleton().GetNetworks(&numNetworks, &networks);
	if (PARTY_FAILED(error))
	{
		PARTY_DBG_TRACE("PlayFabPartyManager::IsRemoteUserMuted(): GetNetworks() failed with error %s\n", GetErrorMessage(error));
		is_muted = false;	// default return value
	}
	else
	{
		for (uint32_t i = 0; i < numNetworks; i++)
		{
			uint32_t numChatControls;
			PartyChatControlArray chatControls;
			error = networks[i]->GetChatControls(&numChatControls, &chatControls);
			if (PARTY_FAILED(error))
			{
				PARTY_DBG_TRACE("PlayFabPartyManager::IsRemoteUserMuted(): GetChatControls() failed with error %s\n", GetErrorMessage(error));
				continue;
			}

			for (uint32_t j = 0; j < numChatControls; j++)
			{
				PartyLocalChatControl* localChatControl = NULL;
				error = chatControls[j]->GetLocal(&localChatControl);
				if (PARTY_FAILED(error))
				{
					PARTY_DBG_TRACE("PlayFabPartyManager::IsRemoteUserMuted(): GetLocal() failed with error %s\n", GetErrorMessage(error));
					continue;
				}

				if (localChatControl != NULL)
				{
					const char* entityID = NULL;
					error = localChatControl->GetEntityId(&entityID);
					if (PARTY_FAILED(error))
					{
						PARTY_DBG_TRACE("PlayFabPartyManager::IsRemoteUserMuted(): GetEntityId() failed with error %s\n", GetErrorMessage(error));
						continue;
					}

					if ((get_for_all_users) || (strcmp(entityID, localUserEntityID) == 0))
					{
						for (uint32_t k = 0; k < numChatControls; k++)
						{
							if (k == j)
								continue;

							PartyChatControl* remoteChatControl = chatControls[k];		// note that this isn't necessarily remote (could be another local control for a different user)

							const char* otherEntityID = NULL;
							error = localChatControl->GetEntityId(&otherEntityID);
							if (PARTY_FAILED(error))
							{
								PARTY_DBG_TRACE("PlayFabPartyManager::IsRemoteUserMuted(): GetEntityId() failed with error %s\n", GetErrorMessage(error));
								continue;
							}

							if ((get_for_all_remote_users) || (strcmp(otherEntityID, remoteUserEntityID) == 0))
							{
								PartyBool chatControlMuted = true;
								error = localChatControl->GetIncomingAudioMuted(remoteChatControl, &chatControlMuted);
								if (PARTY_FAILED(error))
								{
									PARTY_DBG_TRACE("PlayFabPartyManager::IsRemoteUserMuted(): GetIncomingAudioMuted() failed with error %s for local chat control %s with remote chat control %s\n", GetErrorMessage(error), entityID, otherEntityID);
									continue;
								}

								if (!chatControlMuted)
								{
									is_muted = false;
								}
							}
						}
					}
				}
			}
		}
	}

	YYFree(localUserEntityID);
	YYFree(remoteUserEntityID);

	return is_muted;
}

int PlayFabPartyManager::SetChatPermissions(uint64_t _local_user, uint64_t _remote_user, uint64_t _permissions)
{
	bool set_for_all_users = false;
	bool set_for_all_remote_users = false;
	const char* localUserEntityID = NULL;
	const char* remoteUserEntityID = NULL;

	if (_local_user == 0)
		set_for_all_users = true;
	else
	{
		localUserEntityID = XSM::GetSessionUserEntityIDFromID(_local_user);
		if (localUserEntityID == NULL)
		{
			PARTY_DBG_TRACE("PlayFabPartyManager::SetChatPermissions(): couldn't get local user entity ID\n");
			return -1;
		}
	}

	if (_remote_user == 0)
		set_for_all_remote_users = true;
	else
	{
		remoteUserEntityID = XSM::GetSessionUserEntityIDFromID(_remote_user);
		if (remoteUserEntityID == NULL)
		{
			YYFree(localUserEntityID);

			PARTY_DBG_TRACE("PlayFabPartyManager::SetChatPermissions(): couldn't get remote user entity ID\n");
			return -1;
		}
	}

	// Loop through all the current networks	
	uint32_t numNetworks = 0;
	PartyNetworkArray networks = NULL;
	PartyError error = PartyManager::GetSingleton().GetNetworks(&numNetworks, &networks);
	if (PARTY_FAILED(error))
	{
		PARTY_DBG_TRACE("PlayFabPartyManager::SetChatPermissions(): GetNetworks() failed with error %s\n", GetErrorMessage(error));
	}
	else
	{
		for (uint32_t i = 0; i < numNetworks; i++)
		{
			uint32_t numChatControls;
			PartyChatControlArray chatControls;
			error = networks[i]->GetChatControls(&numChatControls, &chatControls);
			if (PARTY_FAILED(error))
			{
				PARTY_DBG_TRACE("PlayFabPartyManager::SetChatPermissions(): GetChatControls() failed with error %s\n", GetErrorMessage(error));
				continue;
			}

			for (uint32_t j = 0; j < numChatControls; j++)
			{
				PartyLocalChatControl* localChatControl = NULL;
				error = chatControls[j]->GetLocal(&localChatControl);
				if (PARTY_FAILED(error))
				{
					PARTY_DBG_TRACE("PlayFabPartyManager::SetChatPermissions(): GetLocal() failed with error %s\n", GetErrorMessage(error));
					continue;
				}

				if (localChatControl != NULL)
				{
					const char* entityID = NULL;
					error = localChatControl->GetEntityId(&entityID);
					if (PARTY_FAILED(error))
					{
						PARTY_DBG_TRACE("PlayFabPartyManager::SetChatPermissions(): GetEntityId() failed with error %s\n", GetErrorMessage(error));
						continue;
					}

					uint64_t localUserID = GetUserFromEntityID(entityID);
					XUMuser* localuser = XUM::GetUserFromId(localUserID);
					if (localuser == NULL)
						continue;

					if ((set_for_all_users) || (strcmp(entityID, localUserEntityID) == 0))
					{
						for (uint32_t k = 0; k < numChatControls; k++)
						{
							if (k == j)
								continue;

							PartyChatControl* remoteChatControl = chatControls[k];		// note that this isn't necessarily remote (could be another local control for a different user)

							const char* otherEntityID = NULL;
							error = remoteChatControl->GetEntityId(&otherEntityID);
							if (PARTY_FAILED(error))
							{
								PARTY_DBG_TRACE("PlayFabPartyManager::SetChatPermissions(): GetEntityId() failed with error %s\n", GetErrorMessage(error));
								continue;
							}

							if ((set_for_all_remote_users) || (strcmp(otherEntityID, remoteUserEntityID) == 0))
							{
								uint64_t otherUserID = XSM::GetSessionUserIDFromEntityID(otherEntityID);
								localuser->SetUserChatPermissions(otherUserID, _permissions);

								PartyXblChatPermissionInfo permission_info = GetChatPermissionMask(localUserID, otherUserID);

								error = localChatControl->SetPermissions(remoteChatControl, ((PartyChatPermissionOptions)_permissions) & permission_info.chatPermissionMask);			// filter specified permissions by those allowed for this user
								if (PARTY_FAILED(error))
								{
									PARTY_DBG_TRACE("PlayFabPartyManager::SetChatPermissions(): SetPermissions() failed with error %s for local chat control %s with remote chat control %s\n", GetErrorMessage(error), entityID, otherEntityID);
									continue;
								}
							}
						}
					}
				}
			}
		}
	}

	YYFree(localUserEntityID);
	YYFree(remoteUserEntityID);

	return 0;
}

uint64_t PlayFabPartyManager::GetChatPermissions(uint64_t _local_user, uint64_t _remote_user)
{
	bool get_for_all_users = false;
	bool get_for_all_remote_users = false;
	const char* localUserEntityID = NULL;
	const char* remoteUserEntityID = NULL;

	if (_local_user == 0)
		get_for_all_users = true;			// we'll return the combined maximal set of permissions for the specified remote user for *all* local users
	else
	{
		localUserEntityID = XSM::GetSessionUserEntityIDFromID(_local_user);
		if (localUserEntityID == NULL)
		{
			PARTY_DBG_TRACE("PlayFabPartyManager::IsRemoteUserMuted(): couldn't get local user entity ID\n");
			return 0;
		}
	}

	if (_remote_user == 0)
		get_for_all_remote_users = true;	// we'll return the combined maximal set of permissions for all remote users for the specified local user
	else
	{
		remoteUserEntityID = XSM::GetSessionUserEntityIDFromID(_remote_user);
		if (remoteUserEntityID == NULL)
		{
			YYFree(localUserEntityID);

			PARTY_DBG_TRACE("PlayFabPartyManager::IsRemoteUserMuted(): couldn't get remote user entity ID\n");
			return 0;
		}
	}

	uint64_t permissions = 0;

	// Loop through all the current networks	
	uint32_t numNetworks = 0;
	PartyNetworkArray networks = NULL;
	PartyError error = PartyManager::GetSingleton().GetNetworks(&numNetworks, &networks);
	if (PARTY_FAILED(error))
	{
		PARTY_DBG_TRACE("PlayFabPartyManager::IsRemoteUserMuted(): GetNetworks() failed with error %s\n", GetErrorMessage(error));		
	}
	else
	{
		for (uint32_t i = 0; i < numNetworks; i++)
		{
			uint32_t numChatControls;
			PartyChatControlArray chatControls;
			error = networks[i]->GetChatControls(&numChatControls, &chatControls);
			if (PARTY_FAILED(error))
			{
				PARTY_DBG_TRACE("PlayFabPartyManager::IsRemoteUserMuted(): GetChatControls() failed with error %s\n", GetErrorMessage(error));
				continue;
			}

			for (uint32_t j = 0; j < numChatControls; j++)
			{
				PartyLocalChatControl* localChatControl = NULL;
				error = chatControls[j]->GetLocal(&localChatControl);
				if (PARTY_FAILED(error))
				{
					PARTY_DBG_TRACE("PlayFabPartyManager::IsRemoteUserMuted(): GetLocal() failed with error %s\n", GetErrorMessage(error));
					continue;
				}

				if (localChatControl != NULL)
				{
					const char* entityID = NULL;
					error = localChatControl->GetEntityId(&entityID);
					if (PARTY_FAILED(error))
					{
						PARTY_DBG_TRACE("PlayFabPartyManager::IsRemoteUserMuted(): GetEntityId() failed with error %s\n", GetErrorMessage(error));
						continue;
					}

					if ((get_for_all_users) || (strcmp(entityID, localUserEntityID) == 0))
					{
						for (uint32_t k = 0; k < numChatControls; k++)
						{
							if (k == j)
								continue;

							PartyChatControl* remoteChatControl = chatControls[k];		// note that this isn't necessarily remote (could be another local control for a different user)

							const char* otherEntityID = NULL;
							error = localChatControl->GetEntityId(&otherEntityID);
							if (PARTY_FAILED(error))
							{
								PARTY_DBG_TRACE("PlayFabPartyManager::IsRemoteUserMuted(): GetEntityId() failed with error %s\n", GetErrorMessage(error));
								continue;
							}

							if ((get_for_all_remote_users) || (strcmp(otherEntityID, remoteUserEntityID) == 0))
							{
								PartyChatPermissionOptions partychatpermissions = PartyChatPermissionOptions::None;
								error = localChatControl->GetPermissions(remoteChatControl, &partychatpermissions);
								if (PARTY_FAILED(error))
								{
									PARTY_DBG_TRACE("PlayFabPartyManager::IsRemoteUserMuted(): GetIncomingAudioMuted() failed with error %s for local chat control %s with remote chat control %s\n", GetErrorMessage(error), entityID, otherEntityID);
									continue;
								}

								permissions |= (uint64_t)partychatpermissions;								
							}
						}
					}
				}
			}
		}
	}

	YYFree(localUserEntityID);
	YYFree(remoteUserEntityID);

	return permissions;
}

PartyXblChatPermissionInfo PlayFabPartyManager::GetChatPermissionMask(uint64_t _local_user, uint64_t _remote_user)
{
	PartyXblChatPermissionInfo info = {};
	info.chatPermissionMask = PartyChatPermissionOptions::None;
	info.reason = PartyXblChatPermissionMaskReason::UnknownError;

	uint32_t chatUserCount = 0;
	PartyXblChatUserArray chatUsers = NULL;
	PartyError error = PartyXblManager::GetSingleton().GetChatUsers(&chatUserCount, &chatUsers);
	if (PARTY_FAILED(error))
	{
		PARTY_DBG_TRACE("PlayFabPartyManager::GetChatPermissionMask(): GetChatUsers() failed with error %s\n", GetXblErrorMessage(error));
		return info;
	}

	PartyXblLocalChatUser* localuser = NULL;
	PartyXblChatUser* remoteuser = NULL;

	for (uint32_t i = 0; i < chatUserCount; i++)
	{
		uint64_t user_id = 0;
		error = chatUsers[i]->GetXboxUserId(&user_id);
		if (PARTY_FAILED(error))
		{
			PARTY_DBG_TRACE("PlayFabPartyManager::GetChatPermissionMask(): GetXboxUserId() failed with error %s\n", GetXblErrorMessage(error));
			continue;
		}

		if (user_id == _local_user)
		{
			error = chatUsers[i]->GetLocal(&localuser);
			if (PARTY_FAILED(error))
			{
				PARTY_DBG_TRACE("PlayFabPartyManager::GetChatPermissionMask(): GetLocal() failed with error %s - specified local user %lld is apparently not local\n", GetXblErrorMessage(error), _local_user);
				continue;
			}
		}
		else if (user_id == _remote_user)
		{
			remoteuser = chatUsers[i];
		}
	}

	if (localuser == NULL)
	{
		PARTY_DBG_TRACE("PlayFabPartyManager::GetChatPermissionMask(): Invalid local user specified\n");
		return info;
	}

	if (remoteuser == NULL)
	{
		PARTY_DBG_TRACE("PlayFabPartyManager::GetChatPermissionMask(): Invalid remote user specified\n");
		return info;
	}

	error = localuser->GetRequiredChatPermissionInfo(remoteuser, &info);
	if (PARTY_FAILED(error))
	{
		PARTY_DBG_TRACE("PlayFabPartyManager::GetChatPermissionMask(): GetRequiredChatPermissionInfo() failed with error %s\n", GetXblErrorMessage(error));		
	}

	return info;
}

void PlayFabPartyManager::AddRemoteChatUser(uint64_t _user_id)
{
	// See if this is a local user
	XUMuser* xumuser = XUM::GetUserFromId(_user_id);
	if (xumuser != NULL)
		return;				// don't add a local user as remote
	
	// First see if we've already created this remote user
	uint32_t userCount;
	PartyXblChatUserArray chatUsers;

	PartyError error = PartyXblManager::GetSingleton().GetChatUsers(&userCount, &chatUsers);
	if (PARTY_FAILED(error))
	{
		PARTY_DBG_TRACE("PlayFabPartyManager::AddRemoteChatUser(): GetChatUsers() failed with error %s\n", GetXblErrorMessage(error));
		return;
	}

	for (uint32_t i = 0; i < userCount; i++)
	{
		PartyXblChatUser* chatUser = chatUsers[i];
		if (chatUser != NULL)
		{
			uint64_t chatUserID = 0;
			error = chatUser->GetXboxUserId(&chatUserID);
			if (PARTY_FAILED(error))
			{
				PARTY_DBG_TRACE("PlayFabPartyManager::AddRemoteChatUser(): GetXboxUserId() failed with error %s\n", GetXblErrorMessage(error));
				continue;
			}

			if (chatUserID == _user_id)
			{
				// Increment refcount
				void* context = NULL;				
				error = chatUser->GetCustomContext(&context);
				if (PARTY_FAILED(error))
				{
					PARTY_DBG_TRACE("PlayFabPartyManager::AddRemoteChatUser(): GetCustomContext() failed with error %s\n", GetXblErrorMessage(error));
					return;
				}

				int refcount = (int)(intptr_t)context;
				refcount++;

				error = chatUser->SetCustomContext((void*)(intptr_t)refcount);
				if (PARTY_FAILED(error))
				{
					PARTY_DBG_TRACE("PlayFabPartyManager::AddRemoteChatUser(): SetCustomContext() failed with error %s\n", GetXblErrorMessage(error));
					return;
				}

				return;
			}
		}
	}

	// We didn't find the user, so create a new one
	PartyXblChatUser* remoteChatUser = NULL;
	error = PartyXblManager::GetSingleton().CreateRemoteChatUser(_user_id, &remoteChatUser);
	if (PARTY_FAILED(error))
	{
		PARTY_DBG_TRACE("PlayFabPartyManager::AddRemoteChatUser(): CreateRemoteChatUser() failed with error %s\n", GetXblErrorMessage(error));
		return;
	}
	if (remoteChatUser == NULL)
	{
		PARTY_DBG_TRACE("PlayFabPartyManager::AddRemoteChatUser(): CreateRemoteChatUser() created NULL chat user\n");
		return;
	}
	int newrefcount = 1;
	error = remoteChatUser->SetCustomContext((void*)(intptr_t)newrefcount);
	if (PARTY_FAILED(error))
	{
		PARTY_DBG_TRACE("PlayFabPartyManager::AddRemoteChatUser(): SetCustomContext() failed with error %s\n", GetXblErrorMessage(error));
		return;
	}
}

void PlayFabPartyManager::RemoveRemoteChatUser(uint64_t _user_id)
{
	// See if we can find this user
	uint32_t userCount;
	PartyXblChatUserArray chatUsers;

	PartyError error = PartyXblManager::GetSingleton().GetChatUsers(&userCount, &chatUsers);
	if (PARTY_FAILED(error))
	{
		PARTY_DBG_TRACE("PlayFabPartyManager::RemoveRemoteChatUser(): GetChatUsers() failed with error %s\n", GetXblErrorMessage(error));
		return;
	}

	for (uint32_t i = 0; i < userCount; i++)
	{
		PartyXblChatUser* chatUser = chatUsers[i];
		if (chatUser != NULL)
		{			
			uint64_t chatUserID = 0;
			error = chatUser->GetXboxUserId(&chatUserID);
			if (PARTY_FAILED(error))
			{
				PARTY_DBG_TRACE("PlayFabPartyManager::RemoveRemoteChatUser(): GetXboxUserId() failed with error %s\n", GetXblErrorMessage(error));
				continue;
			}

			if (chatUserID == _user_id)
			{
				// Check to see if the user is local (we won't remove it if it is)
				PartyXblLocalChatUser* localChatUser;
				error = chatUser->GetLocal(&localChatUser);
				if (PARTY_FAILED(error))
				{
					PARTY_DBG_TRACE("PlayFabPartyManager::RemoveRemoteChatUser(): GetLocal() failed with error %s\n", GetXblErrorMessage(error));
					return;
				}
				if (localChatUser != NULL)
				{
					PARTY_DBG_TRACE("PlayFabPartyManager::RemoveRemoteChatUser(): Attempting to remove local chat user %lld\n", _user_id);
					return;
				}

				// Decrement refcount
				void* context = NULL;
				error = chatUser->GetCustomContext(&context);
				if (PARTY_FAILED(error))
				{
					PARTY_DBG_TRACE("PlayFabPartyManager::RemoveRemoteChatUser(): GetCustomContext() failed with error %s\n", GetXblErrorMessage(error));
					return;
				}

				int refcount = (int)(intptr_t)context;
				refcount--;
				
				if (refcount == 0)
				{
					XUM::RemoveUserChatPermissions(_user_id);		// remove the target chat permissions for this user from all local users

					error = PartyXblManager::GetSingleton().DestroyChatUser(chatUser);
					if (PARTY_FAILED(error))
					{
						PARTY_DBG_TRACE("PlayFabPartyManager::RemoveRemoteChatUser(): DestroyChatUser() failed with error %s\n", GetXblErrorMessage(error));
						return;
					}
					return;
				}
				else
				{
					error = chatUser->SetCustomContext((void*)(intptr_t)refcount);
					if (PARTY_FAILED(error))
					{
						PARTY_DBG_TRACE("PlayFabPartyManager::RemoveRemoteChatUser(): SetCustomContext() failed with error %s\n", GetXblErrorMessage(error));
						return;
					}
				}

				return;
			}
		}
	}
}

PartyNetwork* PlayFabPartyManager::GetNetworkFromID(const char* _networkID)
{
	uint32_t numNetworks = 0;
	PartyNetworkArray networks = NULL;
	PartyError error = PartyManager::GetSingleton().GetNetworks(&numNetworks, &networks);
	if (PARTY_FAILED(error))
	{
		PARTY_DBG_TRACE("PlayFabPartyManager::GetNetworkFromID(): GetNetworks() failed with error %s\n", GetErrorMessage(error));
		return NULL;
	}

	for (uint32_t i = 0; i < numNetworks; i++)
	{
		PartyNetworkDescriptor networkDescriptor;
		error = networks[i]->GetNetworkDescriptor(&networkDescriptor);
		if (PARTY_FAILED(error))
		{
			PARTY_DBG_TRACE("PlayFabPartyManager::GetNetworkFromID(): GetNetworkDescriptor() failed with error %s\n", GetErrorMessage(error));	
			return NULL;
		}

		if (strcmp(_networkID, networkDescriptor.networkIdentifier) == 0)
			return networks[i];
	}

	return NULL;
}

PartyNetwork* PlayFabPartyManager::GetNetworkContainingEntity(const char* _entityID)
{
	uint32_t numNetworks = 0;
	PartyNetworkArray networks = NULL;
	PartyError error = PartyManager::GetSingleton().GetNetworks(&numNetworks, &networks);
	if (PARTY_FAILED(error))
	{
		PARTY_DBG_TRACE("PlayFabPartyManager::GetNetworkContainingEntity(): GetNetworks() failed with error %s\n", GetErrorMessage(error));
		return NULL;
	}

	for (uint32_t i = 0; i < numNetworks; i++)
	{
		uint32_t numEndpoints = 0;
		PartyEndpointArray endpoints = NULL;
		error = networks[i]->GetEndpoints(&numEndpoints, &endpoints);
		if (PARTY_FAILED(error))
		{
			PARTY_DBG_TRACE("PlayFabPartyManager::GetNetworkContainingEntity(): GetEndpoints() failed with error %s\n", GetErrorMessage(error));
			return NULL;
		}		

		for (uint32_t j = 0; j < numEndpoints; j++)
		{
			const char* endpointEntityID = NULL;
			error = endpoints[j]->GetEntityId(&endpointEntityID);
			if (PARTY_FAILED(error))
			{
				PARTY_DBG_TRACE("PlayFabPartyManager::GetNetworkContainingEntity(): GetEntityId() failed with error %s\n", GetErrorMessage(error));
				return NULL;
			}

			if (endpointEntityID == NULL)
			{
				PARTY_DBG_TRACE("PlayFabPartyManager::SendPacket(): GetEntityId() returned NULL entity ID\n");
				return NULL;
			}

			if (strcmp(_entityID, endpointEntityID) == 0)
				return networks[i];
		}		
	}

	return NULL;
}

PartyNetwork* PlayFabPartyManager::GetNetworkContainingEntities(const char* _entityID1, const char* _entityID2)
{
	uint32_t numNetworks = 0;
	PartyNetworkArray networks = NULL;
	PartyError error = PartyManager::GetSingleton().GetNetworks(&numNetworks, &networks);
	if (PARTY_FAILED(error))
	{
		PARTY_DBG_TRACE("PlayFabPartyManager::GetNetworkContainingEntities(): GetNetworks() failed with error %s\n", GetErrorMessage(error));
		return NULL;
	}

	for (uint32_t i = 0; i < numNetworks; i++)
	{
		uint32_t numEndpoints = 0;
		PartyEndpointArray endpoints = NULL;
		error = networks[i]->GetEndpoints(&numEndpoints, &endpoints);
		if (PARTY_FAILED(error))
		{
			PARTY_DBG_TRACE("PlayFabPartyManager::GetNetworkContainingEntities(): GetEndpoints() failed with error %s\n", GetErrorMessage(error));
			return NULL;
		}

		bool found1 = false;
		bool found2 = false;
		for (uint32_t j = 0; j < numEndpoints; j++)
		{
			const char* endpointEntityID = NULL;
			error = endpoints[j]->GetEntityId(&endpointEntityID);
			if (PARTY_FAILED(error))
			{
				PARTY_DBG_TRACE("PlayFabPartyManager::GetNetworkContainingEntities(): GetEntityId() failed with error %s\n", GetErrorMessage(error));
				return NULL;
			}

			if (endpointEntityID == NULL)
			{
				PARTY_DBG_TRACE("PlayFabPartyManager::SendPacket(): GetEntityId() returned NULL entity ID\n");
				return NULL;
			}

			if (strcmp(_entityID1, endpointEntityID) == 0)
				found1 = true;

			if (strcmp(_entityID2, endpointEntityID) == 0)
				found2 = true;

			if (found1 && found2)
				return networks[i];
		}		
	}

	return NULL;
}

bool IsPayloadOfType(void* _payload, PlayFabPayloadType _type)
{
	if (_payload == NULL)
		return false;

	PlayFabPayloadBase* payload = (PlayFabPayloadBase*)_payload;
	if (payload->type == _type)
		return true;

	return false;
}

bool IsPayloadOfSubtype(void* _payload, PlayFabPayloadSubtype _subtype)
{
	if (_payload == NULL)
		return false;

	PlayFabPayloadBase* payload = (PlayFabPayloadBase*)_payload;
	if (payload->subtype == _subtype)
		return true;

	return false;
}
