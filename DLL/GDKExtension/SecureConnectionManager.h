#ifndef SECURE_CONNECTION_MANAGER_H
#define SECURE_CONNECTION_MANAGER_H

#include <mutex>

//#define NO_SECURE_CONNECTION

//#define SCM_VERBOSE_TRACE

enum
{	
	SC_CreateState_Creating,
	SC_CreateState_Created,
	SC_CreateState_Destroying,
	SC_CreateState_Destroyed,
	SC_CreateState_CreationFailed,
};

class SecureConnection
{
public:
	SecureConnection() : refcount(0), state(SC_CreateState_Creating) {}// { association = nullptr; refcount = 0; addressBase64 = nullptr; assocTemplate = nullptr; state = SC_CreateState_Creating; }	// start refcount at zero until the association is created
	virtual ~SecureConnection();

	//SecureDeviceAssociation^ association;	

	int refcount;

	//Platform::String^ addressBase64;
	//SecureDeviceAssociationTemplate^ assocTemplate;

	//Windows::Foundation::EventRegistrationToken assocChangeToken;

	void SetState(int _state) { state = _state; }
	int GetState() { return state; }	

	void OnRemoved();

private:
	volatile int state;	
};

class SCM
{
	friend struct SCM_AutoMutex;

public:
	static void Init();
	static void Quit();

	//static SecureConnection^ GetConnection(Platform::String^ _secureDeviceAddressBase64, SecureDeviceAssociationTemplate^ _template);
	//static SecureConnection^ GetConnection(SecureDeviceAddress^ _address, SecureDeviceAssociationTemplate^ _template);

	//static SecureConnection^ CreateConnection(SecureDeviceAddress^ _address, SecureDeviceAssociationTemplate^ _template);
	//static void DestroyConnection(SecureDeviceAddress^ _address, SecureDeviceAssociationTemplate^ _template);
	//static void RemoveConnection(SecureDeviceAddress^ _address, SecureDeviceAssociationTemplate^ _template);

	//static SecureConnection^ AddAssociation(SecureDeviceAssociation^ _assoc);
	//static void RemoveAssociation(SecureDeviceAssociation^ _assoc);

	//static Windows::Foundation::Collections::IVector<SecureConnection^> ^GetConnections();

	static void LockMutex();
	static void UnlockMutex();

	//static void OnAssociationIncoming(SecureDeviceAssociationTemplate^ sender, SecureDeviceAssociationIncomingEventArgs^ arg);
	//static void OnSecureDeviceAssociationChanged(SecureDeviceAssociation^ sender, SecureDeviceAssociationStateChangedEventArgs^ arg);

private:
	//static Windows::Foundation::Collections::IVector<SecureConnection^> ^connections;
	//static Windows::Foundation::Collections::IMap<Platform::String^, Windows::Foundation::EventRegistrationToken> ^templateEventTokens;

	static std::mutex mutex;
};

struct SCM_AutoMutex
{
	SCM_AutoMutex()
	{
		SCM::LockMutex();
	}

	~SCM_AutoMutex()
	{
		SCM::UnlockMutex();
	}
};

#define SCM_LOCK_MUTEX SCM_AutoMutex __scm_automutex;

#endif // SECURE_CONNECTION_MANAGER_H