//
// Copyright (C) 2020 Opera Norway AS. All rights reserved.
//
// This file is an original work developed by Opera.
//

#include "pch.h"

#include <algorithm>
#include <ctype.h>
#include <fcntl.h>
#include <ppl.h>
#include <ppltasks.h>
#include <robuffer.h>
#include <stdio.h>
#include <string.h>
#include <Windows.h>
#include <XGameSave.h>

#include "GDKX.h"
#include "UserManagement.h"

#define CONNECTED_STORAGE_WAIT_TIME (250)			// quarter of a millisecond
#define CONNECTED_STORAGE_QUERY_TIME (250)			// quarter of a millisecond

extern char* g_pWorkingDirectory;
extern const char*	GetSavePrePend( void );
extern  void YYGetFullPathName( const char* _pszFilename, char* _pDest, int _sizeof );

void _SplitPathAndName( char* _name, int _size, char* _path, int _pathsize, const char* _pszFileName );
static int _GetSaveFileName(char* _name, int _size, const char* _pszFileName);

eXboxFileError g_CurrentFileError = eXboxFileError_NoError;

eXboxFileError ConvertConnectedStorageError( HRESULT _error )
{
	switch(_error)
	{
		case E_GS_BLOB_NOT_FOUND: return eXboxFileError_BlobNotFound;
		case E_GS_CONTAINER_NOT_IN_SYNC: return eXboxFileError_ContainerNotInSync;
		case E_GS_CONTAINER_SYNC_FAILED: return eXboxFileError_ContainerSyncFailed;
		case E_GS_INVALID_CONTAINER_NAME: return eXboxFileError_InvalidContainerName;
		case E_GS_NO_ACCESS: return eXboxFileError_NoAccess;
		case E_GS_NO_SERVICE_CONFIGURATION: return eXboxFileError_NoServiceConfiguration;
		case E_GS_USER_NOT_REGISTERED_IN_SERVICE: return eXboxFileError_UserNotRegisteredInService;
		case E_GS_OUT_OF_LOCAL_STORAGE: return eXboxFileError_OutOfLocalStorage;
		case E_GS_PROVIDED_BUFFER_TOO_SMALL: return eXboxFileError_ProvidedBufferTooSmall;
		case E_GS_QUOTA_EXCEEDED: return eXboxFileError_QuotaExceeded;
		case E_GS_UPDATE_TOO_BIG: return eXboxFileError_UpdateTooBig;
		case E_GS_USER_CANCELED: return eXboxFileError_UserCanceled;
		case 0: return eXboxFileError_NoError;
	}

	return eXboxFileError_UnknownError;
}

RefCountedGameSaveProvider* _SynchronousGetStorageSpace(XUserLocalId _useroverride = XUserNullUserLocalId, bool _override = false)
{
	RefCountedGameSaveProvider* storageSpace = NULL;
	XUserLocalId user = XUM::GetSaveDataUser();

	if (_override == true)
	{
		user = _useroverride;
	}

	if (user.value == XUserNullUserLocalId.value)
	{
		// Check to see if machine storage is available
		if (XUM::GetMachineStorageStatus() == CSSTATUS_INVALID)
		{
			// Have one go at setting up machine storage (network conditions may have made it fail earlier)
			// Don't want to stall things too long so don't do multiple retries
			XUM::SetupMachineStorage();
		}

		// Wait for the machine storage to be set up (if it hasn't already been)
		// Hoping this isn't going to hang forever
		while(XUM::GetMachineStorageStatus() == CSSTATUS_SETTINGUP)
		{
			Timing_Sleep(CONNECTED_STORAGE_WAIT_TIME);	
		}

		{
			XUM_LOCK_MUTEX;
			storageSpace = XUM::GetMachineStorage();

			if (storageSpace == nullptr)
			{
				g_CurrentFileError = ConvertConnectedStorageError(XUM::GetMachineStorageError());
			}
			else
			{
				IncRefGameSaveProvider(storageSpace);
			}
		}
	}
	else
	{
		// First retrieve our XUMuser object		
		XUM::LockMutex();				
		XUMuser* xumuser = XUM::GetUserFromLocalId(user);		
		XUM::UnlockMutex();

		if (xumuser != NULL)
		{			
			if (xumuser->GetStorageStatus() == CSSTATUS_INVALID)
			{
				// Again, try once to set up storage if it hasn't already been done
				xumuser->SetupStorage();
			}

			while(xumuser->GetStorageStatus() == CSSTATUS_SETTINGUP)
			{
				Timing_Sleep(CONNECTED_STORAGE_WAIT_TIME);
			}

			{
				XUM_LOCK_MUTEX;
				storageSpace = xumuser->GetStorage();

				if (storageSpace == nullptr)
				{
					g_CurrentFileError = ConvertConnectedStorageError(xumuser->GetStorageError());
				}
				else
				{
					IncRefGameSaveProvider(storageSpace);
				}
			}
		}
		else
		{
			g_CurrentFileError = eXboxFileError_UserNotFound;
		}
	}

	return storageSpace;
}

// #############################################################################################
/// Function:<summary>
///             Write the INI file to disk...
///          </summary>
///
/// In:		 <param name="_pszFileName"></param>
///			 <param name="_pFileBuffer"></param>
///			 <param name="_FileSize"></param>
/// Out:	 <returns>
///				TRUE for okay, FALSE for error.
///			 </returns>
// #############################################################################################
bool LoadSave_WriteFile( const char* _pszFileName, const char* _pFileBuffer, int _FileSize, ASYNC_SAVE_LOAD_REQ_CONTEXT *context)
{
	char name[2048];
	char pathname[2048];
	char filename[2048];
	HRESULT hr;

	g_CurrentFileError = eXboxFileError_NoError;	// clear error

	_GetSaveFileName( name, 2047, _pszFileName);
	_SplitPathAndName( filename, 2047, pathname, 2047, name );

	// This'll be a synchronous operation to match the expected GML flow

	// Just use the save file name as the container name
	if (_FileSize > (16 * 1024 * 1024))
	{
		DebugConsoleOutput( "Can't write file bigger than 16 megabytes (size: %d, filename: %s)\n", _FileSize, _pszFileName);
		return false;
	}

	RefCountedGameSaveProvider* storageSpace = _SynchronousGetStorageSpace();

	if (storageSpace == nullptr)
		return false;
	
	// Create a container (give it the path name)
	// UPDATE: treat containers like directories - do we need to put a dummy blob in to empty ones?

	XGameSaveContainerHandle container;

	hr = XGameSaveCreateContainer(storageSpace->provider, pathname, &container);
	if (FAILED(hr))
	{
		ReleaseConsoleOutput("Error attempting to write file %s (0x%x)\n", _pszFileName, hr);
		g_CurrentFileError = ConvertConnectedStorageError(hr);
		DecRefGameSaveProvider(storageSpace);
		return false;
	}

	if (container == NULL)
	{
		ReleaseConsoleOutput("Unknown error attempting to write file %s\n", _pszFileName);
		g_CurrentFileError = eXboxFileError_UnknownError;
		DecRefGameSaveProvider(storageSpace);
		return false;
	}

	XGameSaveUpdateHandle updates;
	hr = XGameSaveCreateUpdate(container, pathname, &updates);
	if (FAILED(hr))
	{
		ReleaseConsoleOutput("Error attempting to write file %s (0x%x)\n", _pszFileName, hr);
		g_CurrentFileError = ConvertConnectedStorageError(hr);
		XGameSaveCloseContainer(container);
		DecRefGameSaveProvider(storageSpace);
		return false;
	}

	if (updates == NULL)
	{
		ReleaseConsoleOutput("Unknown error attempting to write file %s\n", _pszFileName);
		g_CurrentFileError = eXboxFileError_UnknownError;
		XGameSaveCloseContainer(container);
		DecRefGameSaveProvider(storageSpace);
		return false;
	}

	hr = XGameSaveSubmitBlobWrite(updates, filename, (const uint8_t*)_pFileBuffer, _FileSize);
	if (FAILED(hr))
	{
		ReleaseConsoleOutput("Error attempting to write file %s (0x%x)\n", _pszFileName, hr);
		g_CurrentFileError = ConvertConnectedStorageError(hr);
		XGameSaveCloseUpdate(updates);
		XGameSaveCloseContainer(container);
		DecRefGameSaveProvider(storageSpace);
		return false;
	}

	hr = XGameSaveSubmitUpdate(updates);
	if (FAILED(hr))
	{
		ReleaseConsoleOutput("Error attempting to write file %s (0x%x)\n", _pszFileName, hr);
		g_CurrentFileError = ConvertConnectedStorageError(hr);
		XGameSaveCloseUpdate(updates);
		XGameSaveCloseContainer(container);
		DecRefGameSaveProvider(storageSpace);
		return false;
	}

	XGameSaveCloseUpdate(updates);
	XGameSaveCloseContainer(container);
	DecRefGameSaveProvider(storageSpace);
	return true;	
}

char* _ReadSaveFile( const char* _pszFileName, int *_fileSize)
{
	// This'll be a synchronous operation to match the expected GML flow
	char pathname[2048];
	char filename[2048];
	HRESULT hr;
	
	_SplitPathAndName( filename, 2047, pathname, 2047, _pszFileName );

	g_CurrentFileError = eXboxFileError_NoError;	// clear error

	RefCountedGameSaveProvider* storageSpace = _SynchronousGetStorageSpace();

	if (storageSpace == nullptr)
		return NULL;

	// Create a container (give it the path name)	
	XGameSaveContainerHandle container;

	hr = XGameSaveCreateContainer(storageSpace->provider, pathname, &container);
	if (FAILED(hr))
	{
		ReleaseConsoleOutput("Error attempting to read file %s (0x%x)\n", _pszFileName, hr);
		g_CurrentFileError = ConvertConnectedStorageError(hr);
		DecRefGameSaveProvider(storageSpace);
		return NULL;
	}

	if (container == NULL)
	{
		ReleaseConsoleOutput("Unknown error attempting to read file %s\n", _pszFileName);
		g_CurrentFileError = eXboxFileError_UnknownError;
		DecRefGameSaveProvider(storageSpace);
		return NULL;
	}
	
	// for each item in the container invoke the callback to figure out if this is the file we're after
	struct BlobInfo
	{
		char* pName;
		int totalallocsize;
	};

	BlobInfo info = { filename, 0 };
	hr = XGameSaveEnumerateBlobInfo(container, &info, [](_In_ const XGameSaveBlobInfo* info, _In_ void* context)
		{
			BlobInfo* myinfo = (BlobInfo*)(context);
			if (strcmp(info->name, myinfo->pName) == 0)
			{
				myinfo->totalallocsize = info->size + (strlen(info->name) + 1) + sizeof(XGameSaveBlob);

				// Found it - returning false to stop enumerating
				return false;
			}

			// keep enumerating
			return true;
		});

	if (FAILED(hr))
	{
		ReleaseConsoleOutput("Error looking for file %s in container (0x%x)\n", _pszFileName, hr);
		g_CurrentFileError = ConvertConnectedStorageError(hr);
		XGameSaveCloseContainer(container);
		DecRefGameSaveProvider(storageSpace);
		return NULL;
	}

	if (info.totalallocsize == 0)
	{
		ReleaseConsoleOutput("Couldn't find file %s in container\n", _pszFileName);
		g_CurrentFileError = eXboxFileError_BlobNotFound;
		XGameSaveCloseContainer(container);
		DecRefGameSaveProvider(storageSpace);
		return NULL;
	}

	int allocsize = info.totalallocsize;
	XGameSaveBlob* blob = (XGameSaveBlob*)(YYAlloc(allocsize));
	uint32_t countofblobs = 1;
	char* blobname = (char*)(filename);
	hr = XGameSaveReadBlobData(container, (const char**)&blobname, &countofblobs, allocsize, blob);
	if (FAILED(hr))
	{
		ReleaseConsoleOutput("Error reading file %s in container (0x%x)\n", _pszFileName, hr);
		g_CurrentFileError = ConvertConnectedStorageError(hr);
		XGameSaveCloseContainer(container);
		DecRefGameSaveProvider(storageSpace);
		return NULL;
	}

	if (countofblobs != 1)
	{
		ReleaseConsoleOutput("Error reading file %s in container\n", _pszFileName);
		g_CurrentFileError = eXboxFileError_UnknownError;
		XGameSaveCloseContainer(container);
		DecRefGameSaveProvider(storageSpace);
		return NULL;
	}

	char* pMem = (char*)YYAlloc(blob->info.size);
	memcpy(pMem, blob->data, blob->info.size);

	if (_fileSize != NULL)
	{
		*_fileSize = blob->info.size;
	}

	YYFree(blob);
	XGameSaveCloseContainer(container);
	DecRefGameSaveProvider(storageSpace);
	return pMem;

}

// #############################################################################################
/// Function:<summary>
///             Read a file (from save area) into memory (allocating memory for the buffer)
///          </summary>
///
/// In:		 <param name="_pszFileName"></param>
///			 <param name="_pFileBuffer"></param>
///			 <param name="_FileSize"></param>
/// Out:	 <returns>
///				TRUE for okay, FALSE for error.
///			 </returns>
// #############################################################################################
char* LoadSave_ReadSaveFile( const char* _pszFileName, int *_fileSize )
{
	char name[2048];
	_GetSaveFileName( name, sizeof(name), _pszFileName );
	
	//return _ReadFile( name, _fileSize );
	return _ReadSaveFile(name, _fileSize);
}

bool _SaveFileExists(const char* _pszFileName)
{
	// This'll be a synchronous operation to match the expected GML flow
	char pathname[2048];
	char filename[2048];
	HRESULT hr;
	
	_SplitPathAndName( filename, 2047, pathname, 2047, _pszFileName );

	g_CurrentFileError = eXboxFileError_NoError;	// clear error

	RefCountedGameSaveProvider* storageSpace = _SynchronousGetStorageSpace();

	if (storageSpace == nullptr)
		return false;

	// First search for container with correct name
	struct ContainerInfo
	{
		char* pName;
		bool found;
		int numblobs;
	};

	ContainerInfo containerinfo = { pathname, false, 0 };
	hr = XGameSaveEnumerateContainerInfo(storageSpace->provider, &containerinfo, [](_In_ const XGameSaveContainerInfo* info, _In_ void* context)
		{
			ContainerInfo* myinfo = (ContainerInfo*)(context);
			if (strcmp(info->name, myinfo->pName) == 0)
			{
				myinfo->found = true;
				myinfo->numblobs = info->blobCount;				

				// Found it - returning false to stop enumerating
				return false;
			}

			// keep enumerating
			return true;
		});

	if (FAILED(hr))
	{
		ReleaseConsoleOutput("Error looking for container %s (0x%x)\n", pathname, hr);
		g_CurrentFileError = ConvertConnectedStorageError(hr);		
		DecRefGameSaveProvider(storageSpace);
		return false;
	}

	if (containerinfo.found == false)
	{
		DebugConsoleOutput("Couldn't find container %s\n", pathname);
		g_CurrentFileError = eXboxFileError_BlobNotFound;
		DecRefGameSaveProvider(storageSpace);
		return false;
	}

	if (containerinfo.numblobs == 0)
	{
		DebugConsoleOutput("Couldn't find file %s in container\n", _pszFileName);
		g_CurrentFileError = eXboxFileError_BlobNotFound;		
		DecRefGameSaveProvider(storageSpace);
		return false;
	}

	// Okay, so we've found the container we're interested in, so open it
	// Create a container (give it the path name)	
	XGameSaveContainerHandle container;

	hr = XGameSaveCreateContainer(storageSpace->provider, pathname, &container);
	if (FAILED(hr))
	{
		ReleaseConsoleOutput("Error attempting to open container %s (0x%x)\n", pathname, hr);
		g_CurrentFileError = ConvertConnectedStorageError(hr);
		DecRefGameSaveProvider(storageSpace);
		return false;
	}

	if (container == NULL)
	{
		ReleaseConsoleOutput("Unknown error attempting to read container %s\n", pathname);
		g_CurrentFileError = eXboxFileError_UnknownError;
		DecRefGameSaveProvider(storageSpace);
		return false;
	}

	// Now look inside the container to see if we can find the blob we're after
	// for each item in the container invoke the callback to figure out if this is the file we're after
	struct BlobInfo
	{
		char* pName;
		bool found;
	};

	BlobInfo blobinfo = { filename, false };
	hr = XGameSaveEnumerateBlobInfo(container, &blobinfo, [](_In_ const XGameSaveBlobInfo* info, _In_ void* context)
		{
			BlobInfo* myinfo = (BlobInfo*)(context);
			if (strcmp(info->name, myinfo->pName) == 0)
			{
				myinfo->found = true;

				// Found it - returning false to stop enumerating
				return false;
			}

			// keep enumerating
			return true;
		});

	if (FAILED(hr))
	{
		ReleaseConsoleOutput("Error looking for file %s in container (0x%x)\n", _pszFileName, hr);
		g_CurrentFileError = ConvertConnectedStorageError(hr);
		XGameSaveCloseContainer(container);
		DecRefGameSaveProvider(storageSpace);
		return false;
	}

	if (blobinfo.found == 0)
	{
		DebugConsoleOutput("Couldn't find file %s in container\n", _pszFileName);
		g_CurrentFileError = eXboxFileError_BlobNotFound;
		XGameSaveCloseContainer(container);
		DecRefGameSaveProvider(storageSpace);
		return false;
	}

	// If we've got past all this then we've found the file - yay!
	XGameSaveCloseContainer(container);

	DecRefGameSaveProvider(storageSpace);
	return true;
}


// #############################################################################################
/// Function:<summary>
///             Check that a save file exists
///          </summary>
///
/// In:		 <param name="_pszFileName">file name</param>
/// Out:	 <returns>
///				true if file exists, false otherwise
///			 </returns>
// #############################################################################################
bool LoadSave_SaveFileExists( const char* _pszFileName )
{
	char name[ 2048 ];
	_GetSaveFileName( name, sizeof(name), _pszFileName );
	
	//return _FileExists( name );
	return _SaveFileExists( name );
} // end LoadSave::SaveFileExists

int _GetSaveFileSize( char* _pszFileName )
{
	// This'll be a synchronous operation to match the expected GML flow
	char pathname[2048];
	char filename[2048];
	HRESULT hr;

	_SplitPathAndName(filename, 2047, pathname, 2047, _pszFileName);

	g_CurrentFileError = eXboxFileError_NoError;	// clear error

	RefCountedGameSaveProvider* storageSpace = _SynchronousGetStorageSpace();

	if (storageSpace == nullptr)
		return 0;

	// First search for container with correct name
	struct ContainerInfo
	{
		char* pName;
		bool found;
		int numblobs;
	};

	ContainerInfo containerinfo = { pathname, false, 0 };
	hr = XGameSaveEnumerateContainerInfo(storageSpace->provider, &containerinfo, [](_In_ const XGameSaveContainerInfo* info, _In_ void* context)
		{
			ContainerInfo* myinfo = (ContainerInfo*)(context);
			if (strcmp(info->name, myinfo->pName) == 0)
			{
				myinfo->found = true;
				myinfo->numblobs = info->blobCount;

				// Found it - returning false to stop enumerating
				return false;
			}

			// keep enumerating
			return true;
		});

	if (FAILED(hr))
	{
		ReleaseConsoleOutput("Error looking for container %s (0x%x)\n", pathname, hr);
		g_CurrentFileError = ConvertConnectedStorageError(hr);
		DecRefGameSaveProvider(storageSpace);
		return 0;
	}

	if (containerinfo.found == false)
	{
		DebugConsoleOutput("Couldn't find container %s\n", pathname);
		g_CurrentFileError = eXboxFileError_BlobNotFound;
		DecRefGameSaveProvider(storageSpace);
		return 0;
	}

	if (containerinfo.numblobs == 0)
	{
		DebugConsoleOutput("Couldn't find file %s in container\n", _pszFileName);
		g_CurrentFileError = eXboxFileError_BlobNotFound;
		DecRefGameSaveProvider(storageSpace);
		return 0;
	}

	// Okay, so we've found the container we're interested in, so open it
	// Create a container (give it the path name)	
	XGameSaveContainerHandle container;

	hr = XGameSaveCreateContainer(storageSpace->provider, pathname, &container);
	if (FAILED(hr))
	{
		ReleaseConsoleOutput("Error attempting to open container %s (0x%x)\n", pathname, hr);
		g_CurrentFileError = ConvertConnectedStorageError(hr);
		DecRefGameSaveProvider(storageSpace);
		return 0;
	}

	if (container == NULL)
	{
		ReleaseConsoleOutput("Unknown error attempting to read container %s\n", pathname);
		g_CurrentFileError = eXboxFileError_UnknownError;
		DecRefGameSaveProvider(storageSpace);
		return 0;
	}

	// Now look inside the container to see if we can find the blob we're after
	// for each item in the container invoke the callback to figure out if this is the file we're after
	struct BlobInfo
	{
		char* pName;		
		int filesize;
	};

	BlobInfo blobinfo = { filename, 0 };
	hr = XGameSaveEnumerateBlobInfo(container, &blobinfo, [](_In_ const XGameSaveBlobInfo* info, _In_ void* context)
		{
			BlobInfo* myinfo = (BlobInfo*)(context);
			if (strcmp(info->name, myinfo->pName) == 0)
			{
				myinfo->filesize = info->size;

				// Found it - returning false to stop enumerating
				return false;
			}

			// keep enumerating
			return true;
		});

	if (FAILED(hr))
	{
		ReleaseConsoleOutput("Error looking for file %s in container (0x%x)\n", _pszFileName, hr);
		g_CurrentFileError = ConvertConnectedStorageError(hr);
		XGameSaveCloseContainer(container);
		DecRefGameSaveProvider(storageSpace);
		return 0;
	}

	if (blobinfo.filesize == 0)
	{
		DebugConsoleOutput("Couldn't find file %s in container\n", _pszFileName);
		g_CurrentFileError = eXboxFileError_BlobNotFound;
		XGameSaveCloseContainer(container);
		DecRefGameSaveProvider(storageSpace);
		return false;
	}

	// If we've got past all this then we've found the file - yay!
	XGameSaveCloseContainer(container);

	DecRefGameSaveProvider(storageSpace);
	return blobinfo.filesize;
}


// #############################################################################################
/// Function:<summary>
///             Get the Size of a File in the Save area
///          </summary>
///
/// In:		 <param name="_pszFileName">name of the file</param>
/// Out:	 <returns>
///				size of the file - (-1 if the file does not exist)
///			 </returns>
// #############################################################################################
int LoadSave_GetSaveFileSize( const char* _pszFileName )
{
	char name[ 2048 ];
	_GetSaveFileName( name, sizeof(name), _pszFileName );
	
	//return _GetSize( name );
	return _GetSaveFileSize( name );
} // end LoadSave::GetSaveFileSize


int _RemoveSaveFile( char* _pszFileName)
{
	// This'll be a synchronous operation to match the expected GML flow
	char pathname[2048];
	char filename[2048];
	HRESULT hr;

	_SplitPathAndName(filename, 2047, pathname, 2047, _pszFileName);

	g_CurrentFileError = eXboxFileError_NoError;	// clear error

	RefCountedGameSaveProvider* storageSpace = _SynchronousGetStorageSpace();

	if (storageSpace == nullptr)
		return 0;

	// First search for container with correct name
	struct ContainerInfo
	{
		char* pName;
		bool found;
		int numblobs;
	};

	ContainerInfo containerinfo = { pathname, false, 0 };
	hr = XGameSaveEnumerateContainerInfo(storageSpace->provider, &containerinfo, [](_In_ const XGameSaveContainerInfo* info, _In_ void* context)
		{
			ContainerInfo* myinfo = (ContainerInfo*)(context);
			if (strcmp(info->name, myinfo->pName) == 0)
			{
				myinfo->found = true;
				myinfo->numblobs = info->blobCount;

				// Found it - returning false to stop enumerating
				return false;
			}

			// keep enumerating
			return true;
		});

	if (FAILED(hr))
	{
		ReleaseConsoleOutput("Error looking for container %s (0x%x)\n", pathname, hr);
		g_CurrentFileError = ConvertConnectedStorageError(hr);
		DecRefGameSaveProvider(storageSpace);
		return 0;
	}

	if (containerinfo.found == false)
	{
		DebugConsoleOutput("Couldn't find container %s\n", pathname);
		g_CurrentFileError = eXboxFileError_BlobNotFound;
		DecRefGameSaveProvider(storageSpace);
		return 0;
	}

	if (containerinfo.numblobs == 0)
	{
		DebugConsoleOutput("Couldn't find file %s in container\n", _pszFileName);
		g_CurrentFileError = eXboxFileError_BlobNotFound;
		DecRefGameSaveProvider(storageSpace);
		return 0;
	}

	// Okay, so we've found the container we're interested in, so open it
	// Create a container (give it the path name)	
	XGameSaveContainerHandle container;

	hr = XGameSaveCreateContainer(storageSpace->provider, pathname, &container);
	if (FAILED(hr))
	{
		ReleaseConsoleOutput("Error attempting to open container %s (0x%x)\n", pathname, hr);
		g_CurrentFileError = ConvertConnectedStorageError(hr);
		DecRefGameSaveProvider(storageSpace);
		return 0;
	}

	if (container == NULL)
	{
		ReleaseConsoleOutput("Unknown error attempting to read container %s\n", pathname);
		g_CurrentFileError = eXboxFileError_UnknownError;
		DecRefGameSaveProvider(storageSpace);
		return 0;
	}

	// Setup update handle
	XGameSaveUpdateHandle updates;
	hr = XGameSaveCreateUpdate(container, pathname, &updates);
	if (FAILED(hr))
	{
		ReleaseConsoleOutput("Error attempting to remove file %s (0x%x)\n", _pszFileName, hr);
		g_CurrentFileError = ConvertConnectedStorageError(hr);
		XGameSaveCloseContainer(container);
		DecRefGameSaveProvider(storageSpace);
		return 0;
	}

	if (updates == NULL)
	{
		ReleaseConsoleOutput("Unknown error attempting to remove file %s\n", _pszFileName);
		g_CurrentFileError = eXboxFileError_UnknownError;
		XGameSaveCloseContainer(container);
		DecRefGameSaveProvider(storageSpace);
		return 0;
	}

	hr = XGameSaveSubmitBlobDelete(updates, filename);
	if (FAILED(hr))
	{
		ReleaseConsoleOutput("Error attempting to remove file %s (0x%x)\n", _pszFileName, hr);
		g_CurrentFileError = ConvertConnectedStorageError(hr);
		XGameSaveCloseUpdate(updates);
		XGameSaveCloseContainer(container);
		DecRefGameSaveProvider(storageSpace);
		return 0;
	}

	hr = XGameSaveSubmitUpdate(updates);
	if (FAILED(hr))
	{
		ReleaseConsoleOutput("Error attempting to remove file %s (0x%x)\n", _pszFileName, hr);
		g_CurrentFileError = ConvertConnectedStorageError(hr);
		XGameSaveCloseUpdate(updates);
		XGameSaveCloseContainer(container);
		DecRefGameSaveProvider(storageSpace);
		return 0;
	}

	XGameSaveCloseUpdate(updates);
	XGameSaveCloseContainer(container);

	DecRefGameSaveProvider(storageSpace);
	return 1;
}

int LoadSave_RemoveSaveFile( const char* _pszFileName )
{
	char name[ 2048 ];
	_GetSaveFileName( name, sizeof(name), _pszFileName );

	return _RemoveSaveFile( name );
}

bool LoadSave_SaveDirectoryExists( char* _pszDirName )
{
	char name[ 2048 ];
	_GetSaveFileName( name, sizeof(name), _pszDirName );	

	// This'll be a synchronous operation to match the expected GML flow
	char pathname[2048];
	char filename[2048];
	HRESULT hr;

	_SplitPathAndName(filename, 2047, pathname, 2047, name);	

	g_CurrentFileError = eXboxFileError_NoError;	// clear error

	RefCountedGameSaveProvider* storageSpace = _SynchronousGetStorageSpace();

	if (storageSpace == nullptr)
		return false;

	// First search for container with correct name
	struct ContainerInfo
	{
		char* pName;
		bool found;		
	};

	ContainerInfo containerinfo = { pathname, false };
	hr = XGameSaveEnumerateContainerInfo(storageSpace->provider, &containerinfo, [](_In_ const XGameSaveContainerInfo* info, _In_ void* context)
		{
			ContainerInfo* myinfo = (ContainerInfo*)(context);
			if (strcmp(info->name, myinfo->pName) == 0)
			{
				myinfo->found = true;

				// Found it - returning false to stop enumerating
				return false;
			}

			// keep enumerating
			return true;
		});

	if (FAILED(hr))
	{
		ReleaseConsoleOutput("Error looking for container %s (0x%x)\n", pathname, hr);
		g_CurrentFileError = ConvertConnectedStorageError(hr);
		DecRefGameSaveProvider(storageSpace);
		return false;
	}

	DecRefGameSaveProvider(storageSpace);
	return containerinfo.found;
}

bool LoadSave_IsSaveDirectory( char* _pszDirName )
{
	// Check the start of the dir name to see if it has the save prepend
	const char* pszSavePrePend = GetSavePrePend();
        		
    // if it is a full path name (that starts with the working directory then strip it and add on the save area)
    if (strncmp( _pszDirName, pszSavePrePend, strlen(pszSavePrePend) ) == 0)
	{
		return true;
	}

	return false;
}

// #############################################################################################
/// Function:<summary>
///             Complete a filename for the Save area
///          </summary>
///
/// In:		 <param name="_name">array to put the final name into</param>
///			 <param name="_size">size of the buffer</param>
///			 <param name="_pszFileName">original name</param>
// #############################################################################################
static int _GetSaveFileName( char* _name, int _size, const char* _pszFileName )
{
	if (_pszFileName == NULL) {
		strcpy( _name, "" );
		return -1;
	} // end if
	else {
        
        // get the current directory
        //char* pCurrDir = GetCurrentDir();
        
        // get the full filename
        char pszFullName[ 2048 ];

		strcpy(pszFullName, _pszFileName);

		//make sure all the slashes are the same, or we get problems in EnsureDirectoryCreated
		char* pD = pszFullName;
		while( *pD != '\0' ) {		
			if (*pD == '\\') *pD = '/';
			++pD;
		} // end while

        
        const char* pszSavePrePend = GetSavePrePend();
        
		// if Relative path i.e. not a full path name then add in the save area as a prefix
        // if it is a full path name (that starts with the working directory then strip it and add on the save area)
        if (strncmp( pszFullName, g_pWorkingDirectory, strlen(g_pWorkingDirectory) ) == 0) {
            strcpy( _name, pszSavePrePend );
            strcat( _name, &pszFullName[ strlen(g_pWorkingDirectory) ] );
        } // end if
        else
        // if it is a full path name (that starts with the working directory then strip it and add on the save area)
        if (strncmp( pszFullName, pszSavePrePend, strlen(pszSavePrePend) ) == 0) {
			strcpy( _name, pszFullName);
        } // end if
        else {
            //strcpy( _name, "" );
			// Bolt on the save prepend to the front
			strcpy( _name, pszSavePrePend);
			strcat( _name, pszFullName);
        } // end else
    } // end else
    
	//make sure all the slashes are the same, or we get problems in EnsureDirectoryCreated
	/*char* pD = _name;
	while( *pD != '\0' ) {		
		if (*pD == '\\') *pD = '/';
		++pD;
	} // end while*/

	// Also, make sure we don't have multiple slashes in a row
	if (strlen(_name) > 1)
	{
		char* pD = &(_name[1]);
		while( *pD != '\0')
		{
			// Really crappy algorithm			
			if ((*pD == '/') && (*(pD-1) == '/'))
			{
				// Shuffle down
				int num = strlen(pD)+1;
				for(int i = 0; i < num; i++)
				{
					*(pD - 1 + i) = pD[i];
				}
			}
			else
			{
				pD++;
			}
		}
	}

	return 0;
}

// #############################################################################################
/// Function:<summary>
///             Get the path and file name as seperate strings
///          </summary>
///
/// In:		 <param name="_name">array to put the file name into</param>
///			 <param name="_size">size of the buffer</param>
///			 <param name="_path">array to put the path into</param>
///			 <param name="_pathsize">size of the path name buffer</param>
///			 <param name="_pszFileName">original name</param>
// #############################################################################################
void _SplitPathAndName( char* _name, int _size, char* _path, int _pathsize, const char* _pszFileName )
{
	// Find last slash in path
	int length = strlen(_pszFileName);

	if (length <= 0)
		return;

	int currchar = length - 1;
	while((currchar >= 0) && (_pszFileName[currchar] != '\\') && (_pszFileName[currchar] != '/'))
	{
		currchar--;
	}

	int splitchar = currchar+1;

	strncpy(_name, &(_pszFileName[splitchar]), _size-1);
	_name[_size-1] = '\0';
	
	if (splitchar > 1)
	{
		int lastchar = std::min(splitchar-1, _pathsize-1);
		strncpy(_path, _pszFileName, lastchar);
		_path[lastchar] = '\0';
	}
	else
	{
		_path[0] = '\0';
	}
}

void AsyncWriteFiles( ASYNC_SAVE_LOAD_REQ_CONTEXT* pAsync )
{
	g_CurrentFileError = eXboxFileError_NoError;

	if (pAsync == NULL)
	{
		g_CurrentFileError = eXboxFileError_UnknownError;
		return;
	}

	if (pAsync->pSaveLoadInfo == NULL)
	{
		g_CurrentFileError = eXboxFileError_UnknownError;

		pAsync->status = 0;	// failed
		pAsync->extendedstatus = g_CurrentFileError;
		pAsync->State = REQ_STATE_COMPLETE;
		pAsync->fInUse = true;
		return;
	}

	// Add up files in async request and make sure their combined size doesn't bust through our limit
	// TODO: queue up multiple requests to work around the limit - in fact, come up with a general way of overcoming the limit (probably by splitting files into multiple blobs and using some sort of naming suffix or index blob to reassemble them on load)
	SAsyncBuffer* pCurrBuff = pAsync->pSaveLoadInfo;
	int totalsize = 0;
	while(pCurrBuff != NULL)
	{
		totalsize += pCurrBuff->size;
		pCurrBuff = pCurrBuff->pNext;
	}	
	
	if (totalsize > (16 * 1024 * 1024))
	{
		if (pAsync->isGroup)
		{
			DebugConsoleOutput( "Can't write file bigger than 16 megabytes (size: %d, groupname: %s)\n", totalsize, pAsync->pGroup);
		}
		else
		{
			DebugConsoleOutput( "Can't write file bigger than 16 megabytes (size: %d, filename: %s)\n", totalsize, pAsync->pSaveLoadInfo->pFilename);
		}

		g_CurrentFileError = eXboxFileError_UpdateTooBig;

		pAsync->status = 0;	// failed
		pAsync->extendedstatus = g_CurrentFileError;
		pAsync->State = REQ_STATE_COMPLETE;
		pAsync->fInUse = true;
		return;
	}	

	auto user = XUM::GetSaveDataUser();

	RefCountedGameSaveProvider* storageSpace = _SynchronousGetStorageSpace(user, true);

	if (storageSpace == nullptr)
	{
		pAsync->status = 0;	// failed
		pAsync->extendedstatus = g_CurrentFileError;
		pAsync->State = REQ_STATE_COMPLETE;
		pAsync->fInUse = true;
		return;
	}

	char name[2048];
	char pathname[2048];
	char filename[2048];

	if (pAsync->isGroup)
	{
		LoadSave::_GetSaveFileName(pathname, 2047, pAsync->pGroup);		// convert group name to a valid save path	
	}
	else
	{
		// there should be only one file in the list, so we'll use the path from that
		LoadSave::_GetSaveFileName(name, 2047, pAsync->pSaveLoadInfo->pFilename);
		_SplitPathAndName(filename, 2047, pathname, 2047, name);
	}

	// Create a container (give it the path name)
	// UPDATE: treat containers like directories - do we need to put a dummy blob in to empty ones?

	XGameSaveContainerHandle container;
	HRESULT hr;

	hr = XGameSaveCreateContainer(storageSpace->provider, pathname, &container);
	if (FAILED(hr))
	{		
		pAsync->status = 0;	// failed
		pAsync->extendedstatus = g_CurrentFileError;
		pAsync->State = REQ_STATE_COMPLETE;
		pAsync->fInUse = true;		
		
		g_CurrentFileError = ConvertConnectedStorageError(hr);
		DecRefGameSaveProvider(storageSpace);
		return;
	}

	if (container == NULL)
	{
		pAsync->status = 0;	// failed
		pAsync->extendedstatus = g_CurrentFileError;
		pAsync->State = REQ_STATE_COMPLETE;
		pAsync->fInUse = true;
		
		g_CurrentFileError = eXboxFileError_UnknownError;
		DecRefGameSaveProvider(storageSpace);
		return;
	}

	XGameSaveUpdateHandle updates;
	hr = XGameSaveCreateUpdate(container, pathname, &updates);
	if (FAILED(hr))
	{		
		pAsync->status = 0;	// failed
		pAsync->extendedstatus = g_CurrentFileError;
		pAsync->State = REQ_STATE_COMPLETE;
		pAsync->fInUse = true;

		g_CurrentFileError = ConvertConnectedStorageError(hr);
		XGameSaveCloseContainer(container);
		DecRefGameSaveProvider(storageSpace);
		return;
	}

	if (updates == NULL)
	{
		pAsync->status = 0;	// failed
		pAsync->extendedstatus = g_CurrentFileError;
		pAsync->State = REQ_STATE_COMPLETE;
		pAsync->fInUse = true;
		
		g_CurrentFileError = eXboxFileError_UnknownError;
		XGameSaveCloseContainer(container);
		DecRefGameSaveProvider(storageSpace);
		return;
	}

	pCurrBuff = pAsync->pSaveLoadInfo;
	while (pCurrBuff != NULL)
	{
		hr = XGameSaveSubmitBlobWrite(updates, pCurrBuff->pFilename, (const uint8_t*)(pCurrBuff->pData), pCurrBuff->size);
		if (FAILED(hr))
		{
			ReleaseConsoleOutput("Error attempting to write file %s (0x%x)\n", pCurrBuff->pFilename, hr);
			g_CurrentFileError = ConvertConnectedStorageError(hr);
			XGameSaveCloseUpdate(updates);
			XGameSaveCloseContainer(container);
			DecRefGameSaveProvider(storageSpace);
			return;
		}

		pCurrBuff = pCurrBuff->pNext;
	}

	struct SubmitUpdateContext
	{
		ASYNC_SAVE_LOAD_REQ_CONTEXT* pAsync;
		XGameSaveUpdateHandle updates;
		XGameSaveContainerHandle container;
		RefCountedGameSaveProvider* storageSpace;		// do we actually need this?		
		XAsyncBlock async;
	};

	SubmitUpdateContext* ctx = new SubmitUpdateContext{};
	if (ctx)
	{
		ctx->pAsync = pAsync;
		ctx->updates = updates;
		ctx->container = container;
		ctx->storageSpace = storageSpace;
		ctx->async.context = ctx;
		ctx->async.callback = [](XAsyncBlock* async)
		{			
			SubmitUpdateContext* ctx = reinterpret_cast<SubmitUpdateContext*>(async->context);
			HRESULT hr = XGameSaveSubmitUpdateResult(async);
			if (FAILED(hr))
			{			
				eXboxFileError error = ConvertConnectedStorageError(hr);
				ctx->pAsync->status = 0;
				ctx->pAsync->extendedstatus = error;
				g_CurrentFileError = error;
			}

			ctx->pAsync->State = REQ_STATE_COMPLETE;
			ctx->pAsync->fInUse = true;

			XGameSaveCloseUpdate(ctx->updates);
			XGameSaveCloseContainer(ctx->container);
			DecRefGameSaveProvider(ctx->storageSpace);
			delete ctx;
		};

		hr = XGameSaveSubmitUpdateAsync(updates, &(ctx->async));
		if (FAILED(hr))
		{
			g_CurrentFileError = ConvertConnectedStorageError(hr);
			XGameSaveCloseUpdate(updates);
			XGameSaveCloseContainer(container);
			DecRefGameSaveProvider(storageSpace);
			delete ctx;
			return;
		}
	}
	else
	{
		XGameSaveCloseUpdate(updates);
		XGameSaveCloseContainer(container);
		DecRefGameSaveProvider(storageSpace);
	}

	return;
}

void AsyncReadFiles( ASYNC_SAVE_LOAD_REQ_CONTEXT* pAsync )
{
	g_CurrentFileError = eXboxFileError_NoError;

	if (pAsync == NULL)
	{
		g_CurrentFileError = eXboxFileError_UnknownError;
		return;
	}

	if (pAsync->pSaveLoadInfo == NULL)
	{
		g_CurrentFileError = eXboxFileError_UnknownError;

		pAsync->status = 0;	// failed
		pAsync->extendedstatus = g_CurrentFileError;
		pAsync->State = REQ_STATE_COMPLETE;
		pAsync->fInUse = true;
		return;
	}

	// Add up files in async request and make sure their combined size doesn't bust through our limit
	// TODO: queue up multiple requests to work around the limit - in fact, come up with a general way of overcoming the limit (probably by splitting files into multiple blobs and using some sort of naming suffix or index blob to reassemble them on load)

	auto user = XUM::GetSaveDataUser();

	char name[2048];
	char pathname[2048];
	char filename[2048];
	HRESULT hr;

	if (pAsync->isGroup)
	{
		LoadSave::_GetSaveFileName(pathname, 2047, pAsync->pGroup);		// convert group name to a valid save path
		strcpy(filename, pAsync->pGroup);		// just to have something valid in it
	}
	else
	{
		// there should be only one file in the list, so we'll use the path from that
		LoadSave::_GetSaveFileName(name, 2047, pAsync->pSaveLoadInfo->pFilename);
		_SplitPathAndName(filename, 2047, pathname, 2047, name);
	}

	g_CurrentFileError = eXboxFileError_NoError;	// clear error

	RefCountedGameSaveProvider* storageSpace = _SynchronousGetStorageSpace();

	if (storageSpace == nullptr)
	{
		// If this failed, try and load it like a normal bundle file
		// (By setting fInUse to true we'll revert to the old behaviour)
		//pAsync->status = g_CurrentFileError;	// failed
		//pAsync->State = REQ_STATE_COMPLETE;
		pAsync->fInUse = true;
		return;
	}

	// Create a container (give it the path name)	
	XGameSaveContainerHandle container;

	hr = XGameSaveCreateContainer(storageSpace->provider, pathname, &container);
	if (FAILED(hr))
	{		
		// If this failed, try and load it like a normal bundle file
		// (By setting fInUse to true we'll revert to the old behaviour)
		//pAsync->status = 0;	// failed
		//pAsync->State = REQ_STATE_COMPLETE;
		pAsync->fInUse = true;

		g_CurrentFileError = ConvertConnectedStorageError(hr);
		DecRefGameSaveProvider(storageSpace);
		return;
	}

	if (container == NULL)
	{
		// If this failed, try and load it like a normal bundle file
		// (By setting fInUse to true we'll revert to the old behaviour)
		//pAsync->status = 0;	// failed
		//pAsync->State = REQ_STATE_COMPLETE;
		pAsync->fInUse = true;
		
		g_CurrentFileError = eXboxFileError_UnknownError;
		DecRefGameSaveProvider(storageSpace);
		return;
	}

	// Create read entries
	int numfiles = 0;

	// First count
	SAsyncBuffer* pCurrBuff = pAsync->pSaveLoadInfo;
	while (pCurrBuff != NULL)
	{
		numfiles++;
		pCurrBuff = pCurrBuff->pNext;
	}
	
	// Now fill in each entry
	char** pFileNames = (char**)YYAlloc(sizeof(char*) * numfiles);
	int currfile = 0;
	pCurrBuff = pAsync->pSaveLoadInfo;
	while (pCurrBuff != NULL)
	{
		const char* name = NULL;
		if (pAsync->isGroup)
		{
			name = pCurrBuff->pFilename;
		}
		else
		{
			name = filename;
		}

		pFileNames[currfile] = (char*)YYAlloc(sizeof(char) * (strlen(name) + 1));
		strcpy(pFileNames[currfile], name);

		currfile++;
		pCurrBuff = pCurrBuff->pNext;
	}

	struct ReadBlobDataContext
	{
		ASYNC_SAVE_LOAD_REQ_CONTEXT* pAsync;		
		XGameSaveContainerHandle container;
		RefCountedGameSaveProvider* storageSpace;		// do we actually need this?	
		char* filename;
		char** pFileNames;
		int numfiles;
		XAsyncBlock async;
	};

	ReadBlobDataContext* ctx = new ReadBlobDataContext{};
	if (ctx)
	{
		ctx->pAsync = pAsync;		
		ctx->container = container;
		ctx->storageSpace = storageSpace;
		ctx->filename = (char*)YYAlloc(sizeof(char) * (strlen(filename) + 1));
		strcpy(ctx->filename, filename);
		ctx->pFileNames = pFileNames;
		ctx->numfiles = numfiles;
		ctx->async.context = ctx;
		ctx->async.callback = [](XAsyncBlock* async)
		{
			ReadBlobDataContext* ctx = reinterpret_cast<ReadBlobDataContext*>(async->context);
			size_t allocSize;
			HRESULT hr = XAsyncGetStatus(async, false);	
			if (SUCCEEDED(hr))
			{				
				hr = XAsyncGetResultSize(async, &allocSize);
			}

			XGameSaveBlob* blobData = NULL;
			uint32_t blobCount = 0;
			if (SUCCEEDED(hr))
			{
				blobData = (XGameSaveBlob*)YYAlloc(allocSize);

				hr = XGameSaveReadBlobDataResult(async, allocSize, blobData, &blobCount);
			}

			if (SUCCEEDED(hr))
			{
				SAsyncBuffer* pCurrBuff = ctx->pAsync->pSaveLoadInfo;
				while (pCurrBuff != NULL)
				{
					const char* blobname = NULL;
					if (ctx->pAsync->isGroup)
					{
						blobname = pCurrBuff->pFilename;
					}
					else
					{
						blobname = ctx->filename;
					}

					for (uint32_t i = 0; i < blobCount; i++)
					{
						XGameSaveBlob* currentBlob = blobData + i;
						if (strcmp(currentBlob->info.name, blobname) == 0)
						{
							IBuffer* pB = GetIBuffer(pCurrBuff->bufferIndex);
							if (pB)
								IBuffer::CopyMemoryToBuffer(pB, (uint8*)currentBlob->data, currentBlob->info.size, 0, pCurrBuff->size, pCurrBuff->offset, true);							
						}
					}

					pCurrBuff = pCurrBuff->pNext;
				}
			}
			else
			{
				eXboxFileError error = ConvertConnectedStorageError(hr);
				ctx->pAsync->status = 0;
				ctx->pAsync->extendedstatus = error;
				g_CurrentFileError = error;
			}

			ctx->pAsync->State = REQ_STATE_COMPLETE;
			ctx->pAsync->fInUse = true;

			YYFree(ctx->filename);
			for (int i = 0; i < ctx->numfiles; i++)
			{
				YYFree(ctx->pFileNames[i]);
			}
			YYFree(ctx->pFileNames);
			XGameSaveCloseContainer(ctx->container);
			DecRefGameSaveProvider(ctx->storageSpace);
			delete ctx;
		};

		hr = XGameSaveReadBlobDataAsync(container, (const char**)(ctx->pFileNames), ctx->numfiles, &ctx->async);
		if (FAILED(hr))
		{
			// If this failed, try and load it like a normal bundle file
			// (By setting fInUse to true we'll revert to the old behaviour)
			//pAsync->status = 0;	// failed
			//pAsync->State = REQ_STATE_COMPLETE;
			pAsync->fInUse = true;

			YYFree(ctx->filename);
			for (int i = 0; i < ctx->numfiles; i++)
			{
				YYFree(ctx->pFileNames[i]);
			}
			YYFree(ctx->pFileNames);			
			delete ctx;

			g_CurrentFileError = eXboxFileError_UnknownError;
			XGameSaveCloseContainer(container);
			DecRefGameSaveProvider(storageSpace);
			return;
		}
	}
}
