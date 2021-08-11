#ifndef CREATESESSION_H
#define CREATESESSION_H

#ifdef YYXBOX
//#define XBOX_SERVICES
#endif

enum
{
	XSMTS_CreateSession_InitialSession = 1,
	XSMTS_CreateSession_SetHost,							
	XSMTS_CreateSession_CreateSuccess,	
	XSMTS_CreateSession_ProcessUpdatedSession,	
	XSMTS_CreateSession_FailureToWrite,
};

class XSMtaskCreateSession : public XSMtaskBase
{
public:
	char* hopperName;	
	char* matchAttributes;

	//Party::PartyNetworkDescriptor networkDescriptor;
	//Party::PartyNetwork* network;
	//char invitationIdentifier[Party::c_maxInvitationIdentifierStringLength + 1];

	XSMtaskCreateSession() { taskType = XSMTT_CreateSession; hopperName = NULL; matchAttributes = NULL; /*network = NULL;*/ }
	virtual ~XSMtaskCreateSession();
	
	virtual void Process() override;
	virtual void ProcessSessionChanged(xbl_session_ptr _updatedsession) override;
	virtual void ProcessSessionDeleted(xbl_session_ptr _sessiontodelete) override;

	virtual void ProcessPlayFabPartyChange(const Party::PartyStateChange* _change) override;
};

#endif