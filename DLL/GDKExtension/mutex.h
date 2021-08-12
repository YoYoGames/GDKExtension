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
#ifndef __MUTEX_H__
#define __MUTEX_H__

#if defined(ANDROID_NDK) || defined(IPAD) || defined(LINUX) || defined(MAC) || defined(TIZEN) || defined(YYPS4)|| defined(YYPS5)
#include <pthread.h>
#define PTHREAD_MUTEX
#endif

#if defined(YYSWITCH)
#include <nn/os.h>
#endif

#if defined(YYPS3)
#include <sys/synchronization.h>
#define PS3_MUTEX
#endif

class	Mutex
{
private:
	void*				m_pMutex;
#ifdef PTHREAD_MUTEX
	pthread_mutex_t		m_hMutex;
#endif
#ifdef PS3_MUTEX
	sys_lwmutex_t				m_idMutex;
	sys_lwmutex_attribute_t		m_attrMutex;
#endif


public:
	Mutex(const char*  _pName );
	~Mutex(void);

	void	Init( const char*  _pName );
	bool	Lock( void );
	void	Unlock( void );
};

struct AutoMutex
{
	Mutex *m_hold;
	const char *m_message;

	AutoMutex(Mutex *m, const char *message)
	{
		m_hold = m;
		if (m)
		{
			m_message = message;
			m->Lock();
		}
	}

	~AutoMutex()
	{
		if (m_hold)
		{
			m_hold->Unlock();
		}
	}
};

#endif	// __MUTEX_H__


