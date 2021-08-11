#ifndef JOINSESSION_H
#define JOINSESSION_H

enum
{
	XSMTS_JoinSession_InitialSession = 1,	
	XSMTS_JoinSession_WaitMatchTicketResult,
	XSMTS_JoinSession_FoundSession,
	XSMTS_JoinSession_ProcessUpdatedSession,
	XSMTS_JoinSession_QOS,
	XSMTS_JoinSession_SetHost,
	XSMTS_JoinSession_ReportMatchSetup,
	XSMTS_JoinSession_GetInitialMemberDetails,
	XSMTS_JoinSession_Completed,
	XSMTS_JoinSession_NoSessionsFound,
	XSMTS_JoinSession_FailureToWrite,
};

class XSMtaskJoinSession : public XSMtaskBase
{
public:
	//Platform::String^ sdaTemplateName;
	const char* sessionHandle;
	xbl_session_ptr joinSession;		

	XSMtaskJoinSession() { taskType = XSMTT_JoinSession; sessionHandle = NULL; reFindingHost = false; }
	
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