// **********************************************************************************************************************
// 
// Copyright (c)2010, YoYo Games. All Rights reserved.
// 
// File:	    	Mutex.h
// Created:	        13/09/2010
// Author:    		Mike
// Project:		    Runner
// Description:   	Simple "mutex" (lock/unlock) for very simple multi-threading.
// 
// Date				Version		BY		Comment
// ----------------------------------------------------------------------------------------------------------------------
// 13/09/2010		V0.1		MJD		1st version
// 
// **********************************************************************************************************************
#include	"mutex.h"
#include	<windows.h>


// #############################################################################################
/// Function:<summary>
///             Create a new MUTEX
///          </summary>
// #############################################################################################
Mutex::Mutex(const char*  _pName )
{
	Init(_pName);
}


// #############################################################################################
/// Function:<summary>
///             delete the mutex
///          </summary>
// #############################################################################################
Mutex::~Mutex(void)
{
	DeleteCriticalSection((CRITICAL_SECTION*)m_pMutex);
	free( m_pMutex );
}

// #############################################################################################
/// Function:<summary>
///             Initialise the object.
///				As this is used in the memory sub-system, DONT use NEW()
///          </summary>
// #############################################################################################
void Mutex::Init( const char*  _pName  )
{
	m_pMutex = (void*) malloc( sizeof(CRITICAL_SECTION) );
	if (!InitializeCriticalSectionAndSpinCount((CRITICAL_SECTION*)m_pMutex, 0x80000400) )  return;
}
// #############################################################################################
/// Function:<summary>
///             Aquire the lock (or wait until it can)
///          </summary>
// #############################################################################################
bool Mutex::Lock( void )
{
	EnterCriticalSection((CRITICAL_SECTION*)m_pMutex); 
	return true;
}

// #############################################################################################
/// Function:<summary>
///             Release the lock 
///          </summary>
// #############################################################################################
void Mutex::Unlock( void )
{
	LeaveCriticalSection((CRITICAL_SECTION*)m_pMutex); 
}



