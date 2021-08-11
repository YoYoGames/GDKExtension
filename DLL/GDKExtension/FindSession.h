#ifndef FINDSESSION_H
#define FINDSESSION_H

enum
{
	XSMTS_FindSession_InitialSession = 1,	
	XSMTS_FindSession_CreateMatchTicket,
	XSMTS_FindSession_WaitMatchTicketResult,
	XSMTS_FindSession_FoundSession,
	XSMTS_FindSession_ProcessUpdatedSession,
	XSMTS_FindSession_QOS,
	XSMTS_FindSession_SetHost,
	XSMTS_FindSession_ReportMatchSetup,
	XSMTS_FindSession_GetInitialMemberDetails,
	XSMTS_FindSession_Completed,
	XSMTS_FindSession_NoSessionsFound,
	XSMTS_FindSession_FailureToWrite,
};

class XSMtaskFindSession : public XSMtaskBase
{
public:
	char* hopperName;
	//Platform::String^ sdaTemplateName;
	char* matchAttributes;
	XblCreateMatchTicketResponse ticketResponse;
	xbl_session_ptr matchedSession;		

	XSMtaskFindSession() { taskType = XSMTT_FindSession; hopperName = NULL; /*sdaTemplateName = nullptr; */ matchAttributes = NULL; ZeroMemory(&ticketResponse, sizeof(XblCreateMatchTicketResponse)); reFindingHost = false; }
	
	virtual void Process() override;
	virtual void ProcessSessionChanged(xbl_session_ptr _updatedsession) override;
	virtual void ProcessSessionDeleted(xbl_session_ptr _sessiontodelete) override;

protected:
	virtual void SignalFailure() override;
private:		
	void SignalNoSessionsFound();
	void SignalSuccess();	

	bool reFindingHost;
};

#endif