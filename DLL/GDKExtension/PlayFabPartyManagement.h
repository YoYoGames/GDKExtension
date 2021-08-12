//
// Copyright (C) 2020 Opera Norway AS. All rights reserved.
//
// This file is an original work developed by Opera.
//

#ifndef PLAYFABPARTYMANAGEMENT_H
#define PLAYFABPARTYMANAGEMENT_H

#include <memory>
#include <stdint.h>
#include <string>
#include <unordered_map>
#include <vector>

#include "Party.h"
#include "PartyXboxLive.h"

enum class PlayFabPayloadType : uint32_t
{
	Unknown = 0,
	SetupLocalUser,
	XSM
};

enum class PlayFabPayloadSubtype : uint32_t
{
	Generic = 0,	
};

struct PlayFabPayloadBase
{
	PlayFabPayloadType type;
	PlayFabPayloadSubtype subtype;

	PlayFabPayloadBase(PlayFabPayloadType _type, PlayFabPayloadSubtype _subtype = PlayFabPayloadSubtype::Generic) : type(_type), subtype(_subtype) {}
	virtual ~PlayFabPayloadBase() {}
};

struct XSMPlayFabPayload;
struct PacketBufferNetwork;
struct NetworkUserInfo;

class PlayFabPartyManager
{
public:
	static int m_initRefCount;

	static bool Init();
	static void Shutdown();

	static bool IsInitialised();
	static void Process();

	static int SetupLocalUser(uint64_t _user_id, int _requestID);
	static bool ShouldRefreshLocalUserLogin(uint64_t _user_id);
	static int RefreshLocalUserLogin(uint64_t _user_id, int _requestID);
	static int CleanupLocalUser(uint64_t _user_id);
	static int CreateNetwork(uint64_t _user_id, int _maxPlayers, XSMPlayFabPayload* _payload);	
	static int ConnectToNetwork(Party::PartyNetworkDescriptor* _networkDescriptor, XSMPlayFabPayload* _payload);
	static int AuthenticateLocalUser(Party::PartyNetwork* _network, uint64_t _user_id, char* _invitationID, XSMPlayFabPayload* _payload);
	static int ConnectChatControl(Party::PartyNetwork* _network, uint64_t _user_id, XSMPlayFabPayload* _payload);
	static int CreateEndpoint(Party::PartyNetwork* _network, uint64_t _user_id, XSMPlayFabPayload* _payload);
	static int SerialiseNetworkDescriptor(Party::PartyNetworkDescriptor* _networkDescriptor, char* _output);
	static int DeserialiseNetworkDescriptor(const char* _input, Party::PartyNetworkDescriptor* _networkDescriptor);
	static const char* GetUserEntityID(uint64_t _user_id);
	static uint64_t GetUserFromEntityID(const char* _entityID);
	static int SendPacket(uint64_t _user_id, const char* _networkID, const char* _targetEntityID, const void* _pBuff, int _size, bool _reliable);	
	static int LeaveNetwork(const char* _networkID);

	static int MuteRemoteUser(uint64_t _local_user, uint64_t _remote_user, bool _mute);
	static bool IsRemoteUserMuted(uint64_t _local_user, uint64_t _remote_user);
	static int SetChatPermissions(uint64_t _local_user, uint64_t _remote_user, uint64_t _permissions);
	static uint64_t GetChatPermissions(uint64_t _local_user, uint64_t _remote_user);

	static Party::PartyXblChatPermissionInfo GetChatPermissionMask(uint64_t _local_user, uint64_t _remote_user);

	static void AddRemoteChatUser(uint64_t _user_id);
	static void RemoveRemoteChatUser(uint64_t _user_id);

	static Party::PartyNetwork* GetNetworkFromID(const char* _networkID);
	static Party::PartyNetwork* GetNetworkContainingEntity(const char* _entityID);
	static Party::PartyNetwork* GetNetworkContainingEntities(const char* _entityID1, const char* _entityID2);

	// PartyXbl Change handlers
	static void OnCreateLocalChatUserCompleted(const Party::PartyXblStateChange* change);
	static void OnLoginToPlayFabCompleted(const Party::PartyXblStateChange* change);
	static void OnRequiredChatPermissionInfoChanged(const Party::PartyXblStateChange* change);

	// Party Change handlers
	static void OnCreateNewNetworkCompleted(const Party::PartyStateChange* change);
	static void OnConnectToNetworkCompleted(const Party::PartyStateChange* change);
	static void OnAuthenticateLocalUserCompleted(const Party::PartyStateChange* change);
	static void OnConnectChatControlCompleted(const Party::PartyStateChange* change);
	static void OnCreateEndpointCompleted(const Party::PartyStateChange* change);
	static void OnEndpointMessageReceived(const Party::PartyStateChange* change);
	static void OnCreateChatControlCompleted(const Party::PartyStateChange* change);
	static void OnSetChatAudioInputCompleted(const Party::PartyStateChange* change);
	static void OnSetChatAudioOutputCompleted(const Party::PartyStateChange* change);

	static void AddToNetworkUsers(const char* _networkID, const char* _sendingEntityID, uint64_t _sendingXuid);
	static void RemoveFromNetworkUsers(const char* _networkID, const char* _sendingEntityID);
	static void RemoveFromNetworkUsers(const char* _networkID, uint64_t _sendingXuid);
	static bool ExistsInNetworkUsers(const char* _networkID, const char* _entityID);
	static bool ExistsInNetworkUsers(const char* _networkID, uint64_t _user_id);

	static void RemoveBufferedPacketsForUser(const char* _networkID, const char* _sendingEntityID);	
	static void RemoveBufferedPacketsForUser(const char* _networkID, uint64_t _sendingXuid);
	static void RemoveBufferedPacketsForNetwork(const char* _networkID);

private:
	static int SendLoopbackPacket(const char* entityID, const char* _targetEntityID, const void* _pBuff, int _size);

	static bool ShouldBuffer(const char* _networkID, const char* _sendingEntityID);
	static void BufferPacket(const char* _networkID, const char* _sendingEntityID, const char* _receivingEntityID, const void* _messageBuffer, uint32_t _messageSize);
	static void FlushBufferedPackets();

	static std::unordered_map<std::string, std::unique_ptr<PacketBufferNetwork>> packetBuffer;
	static std::unordered_map<std::string, std::unique_ptr<NetworkUserInfo>> networkUsers;
};

bool IsPayloadOfType(void* _payload, PlayFabPayloadType _type);
bool IsPayloadOfSubtype(void* _payload, PlayFabPayloadSubtype _subtype);

#endif
