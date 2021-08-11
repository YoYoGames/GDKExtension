#ifndef MIGRATEHOST_H
#define MIGRATEHOST_H

enum
{
	XSMTS_MigrateHost_SetHost,
	XSMTS_MigrateHost_NotifyHostChanged,
	XSMTS_MigrateHost_EstablishConnections,
	XSMTS_MigrateHost_Completed,	
	XSMTS_MigrateHost_FailureToWrite,
};

class XSMtaskMigrateHost : public XSMtaskBase
{
public:	
	//Platform::String^ sdaTemplateName;
	const char* hopperName;
	const char* matchAttributes;

	XSMtaskMigrateHost() { taskType = XSMTT_MigrateHost; /*sdaTemplateName = nullptr;*/ hopperName = NULL; matchAttributes = NULL; }
	
	virtual void Process() override;
	virtual void ProcessSessionChanged(xbl_session_ptr _updatedsession) override;
	virtual void ProcessSessionDeleted(xbl_session_ptr _sessiontodelete) override;

protected:
	virtual void SignalFailure() override; 
private:		
	void SignalHostChanged();	
};

#endif