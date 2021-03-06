//
// Copyright (C) 2020 Opera Norway AS. All rights reserved.
//
// This file is an original work developed by Opera.
//

#ifndef CREATESESSION_H
#define CREATESESSION_H

#include "SessionManagement.h"

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

	XSMtaskCreateSession() { 
		taskType = XSMTT_CreateSession; 
		hopperName = NULL; 
		matchAttributes = NULL; 
		/*network = NULL;*/ 
	} // end constructor
	virtual ~XSMtaskCreateSession();
	
	virtual void Process() override;
	virtual void ProcessSessionChanged(xbl_session_ptr _updatedsession) override;
	virtual void ProcessSessionDeleted(xbl_session_ptr _sessiontodelete) override;

	virtual void ProcessPlayFabPartyChange(const Party::PartyStateChange* _change) override;
};

#endif