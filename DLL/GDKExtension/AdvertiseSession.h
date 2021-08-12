//
// Copyright (C) 2020 Opera Norway AS. All rights reserved.
//
// This file is an original work developed by Opera.
//

#ifndef ADVERTISESESSION_H
#define ADVERTISESESSION_H

enum
{	
	XSMTS_AdvertiseSession_CreateMatchTicket = 1,
	XSMTS_AdvertiseSession_WaitMatchTicketResult,	
	XSMTS_AdvertiseSession_WaitForAvailableSlots,
	XSMTS_AdvertiseSession_FailureToWrite,
};

class XSMtaskAdvertiseSession : public XSMtaskBase
{
public:
	char* hopperName;
	char* matchAttributes;
	XblCreateMatchTicketResponse ticketResponse;

	XSMtaskAdvertiseSession() { 
		taskType = XSMTT_AdvertiseSession; 
		hopperName = NULL; 
		matchAttributes = NULL; 
		ZeroMemory(&ticketResponse, sizeof(XblCreateMatchTicketResponse)); 
	} // end constructor

	virtual ~XSMtaskAdvertiseSession();
	
	virtual void Process() override;
	virtual void ProcessSessionChanged(xbl_session_ptr _updatedsession) override;
	virtual void ProcessSessionDeleted(xbl_session_ptr _sessiontodelete) override;
};

#endif