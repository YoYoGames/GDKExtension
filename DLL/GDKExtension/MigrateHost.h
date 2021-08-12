//
// Copyright (C) 2020 Opera Norway AS. All rights reserved.
//
// This file is an original work developed by Opera.
//
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
	const char* hopperName;
	const char* matchAttributes;

	XSMtaskMigrateHost() { 
		taskType = XSMTT_MigrateHost; 
		hopperName = NULL; 
		matchAttributes = NULL; 
	} // end constructor
	
	virtual void Process() override;
	virtual void ProcessSessionChanged(xbl_session_ptr _updatedsession) override;
	virtual void ProcessSessionDeleted(xbl_session_ptr _sessiontodelete) override;

protected:
	virtual void SignalFailure() override; 
private:		
	void SignalHostChanged();	
};

#endif