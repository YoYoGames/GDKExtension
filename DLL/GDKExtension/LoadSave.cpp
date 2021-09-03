//
// Copyright (C) 2020 Opera Norway AS. All rights reserved.
//
// This file is an original work developed by Opera.
//

#include "pch.h"

#include <algorithm>
#include <ctype.h>
#include <fcntl.h>
#include <inttypes.h>
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

struct PendingSave
{
	const int async_id;

	const std::string name;

	void* data;
	size_t data_size;

	PendingSave(int async_id, const std::string& name, void* data, size_t data_size) :
		async_id(async_id),
		name(name),
		data(data),
		data_size(data_size) {}

	~PendingSave()
	{
		YYFree(data);
	}

	PendingSave(const PendingSave& src) = delete;
	PendingSave& operator=(const PendingSave& src) = delete;
};

static bool save_group_open = false;
static std::string save_group_container_name;
static std::list<PendingSave>* save_group_buffers = NULL;

static void _gdk_save_commit(const std::string& container_name, std::list<PendingSave>* buffers);

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

RefCountedGameSaveProvider* _SynchronousGetStorageSpace(eXboxFileError *error)
{
	RefCountedGameSaveProvider* storageSpace = NULL;
	XUserLocalId user = XUM::GetSaveDataUser();

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
					*error = ConvertConnectedStorageError(xumuser->GetStorageError());
				}
				else
				{
					*error = eXboxFileError_NoError;
					IncRefGameSaveProvider(storageSpace);
				}
			}
		}
		else
		{
			*error = eXboxFileError_UserNotFound;
		}
	}

	return storageSpace;
}

std::pair<std::string, std::string> _SplitPathAndName(const std::string &filename)
{
	size_t last_slash = filename.find_last_of("/\\");

	if (last_slash == std::string::npos)
	{
		return std::make_pair("", filename);
	}
	else {
		return std::make_pair(filename.substr(0, last_slash), filename.substr(last_slash + 1));
	}
}

YYEXPORT
void gdk_save_group_begin(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	if (argc != 1)
	{
		DebugConsoleOutput("gdk_save_group_begin() - error: expected 1 argument (container_name)\n");
		return;
	}

	if (save_group_open)
	{
		DebugConsoleOutput("gdk_save_group_begin() - error: save group is already open\n");
		return;
	}

	const char* container_name = YYGetString(arg, 0);

	save_group_container_name = container_name;
	save_group_buffers = new std::list<PendingSave>;
	
	save_group_open = true;
}

YYEXPORT
void gdk_save_group_end(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	if (!save_group_open)
	{
		DebugConsoleOutput("gdk_save_group_end() - error: no save group opened\n");
		return;
	}

	/* _gdk_save_commit() takes ownership of the save_group_buffers list. */
	_gdk_save_commit(save_group_container_name, save_group_buffers);
	
	save_group_buffers = NULL;
	save_group_open = false;
}

YYEXPORT
void gdk_save_buffer(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_REAL;

	if (argc != 4)
	{
		DebugConsoleOutput("gdk_save_buffer() - error: expected 4 arguments (buffer_idx, filename, offset, length)\n");
		Result.val = -1;

		return;
	}

	int buffer_idx = YYGetInt32(arg, 0);
	const char* filename = YYGetString(arg, 1);
	int offset = YYGetInt32(arg, 2);
	int size = YYGetInt32(arg, 3);

	unsigned char* buffer_data;
	int buffer_size;

	if(!BufferGetContent(buffer_idx, (void**)(&buffer_data), &buffer_size))
	{
		DebugConsoleOutput("gdk_save_buffer() - error: specified buffer not found\n");
		Result.val = -1;

		return;
	}

	if (offset < 0 || offset >= buffer_size || size < 1 || (offset + size) > buffer_size)
	{
		DebugConsoleOutput("gdk_save_buffer() - error: offset and/or size argument out of range\n");
		Result.val = -1;

		YYFree(buffer_data);
		return;
	}

	/* NOTE: Ownership of buffer_data is transferred to PendingSave object. */

	if (save_group_open)
	{
		int async_id = g_HTTP_ID++;
		save_group_buffers->emplace_back(async_id, filename, (buffer_data + offset), size);

		Result.val = async_id;
	}
	else {
		std::string container_name;
		std::string blob_name;
		std::tie(container_name, blob_name) = _SplitPathAndName(filename);

		std::list<PendingSave> *buffers = new std::list<PendingSave>();

		int async_id = g_HTTP_ID++;
		buffers->emplace_back(async_id, blob_name, (buffer_data + offset), size);

		/* _gdk_save_commit() takes ownership of the buffers list. */
		_gdk_save_commit(container_name, buffers);

		Result.val = async_id;
	}
}

static void _gdk_save_commit(const std::string &container_name, std::list<PendingSave> *buffers)
{
	/* ugh... some of the XGameSave methods are marked as unsafe to call on a timing-sensitive
	 * thread, and on top of that, XUM doesn't have a way of telling us when it finishes setting up
	 * a user's XGameSaveProviderHandle, so we need to poll it. Because of this we spawn a new
	 * thread to handle the whole thing and raise the async event when its done.
	*/

	std::thread t([=]()
	{
		std::unique_ptr< std::list<PendingSave> > buffers_guard(buffers);

		eXboxFileError gsp_err;
		RefCountedGameSaveProvider *gsp = _SynchronousGetStorageSpace(&gsp_err);

		if (gsp == NULL)
		{
			ReleaseConsoleOutput("gdk_save_buffer() - error: unable to initialise XGameSaveProvider\n");

			for (auto b = buffers->begin(); b != buffers->end(); ++b)
			{
				int dsMapIndex = CreateDsMap(3,
					"id", (double)(b->async_id), NULL,
					"status", (double)(false), NULL,
					"error", (double)(gsp_err), NULL);
				CreateAsyncEventWithDSMap(dsMapIndex, EVENT_OTHER_ASYNC_SAVE_LOAD);
			}

			return;
		}

		XGameSaveContainerHandle container = NULL;
		XGameSaveUpdateHandle updates = NULL;

		HRESULT hr = XGameSaveCreateContainer(gsp->provider, container_name.c_str(), &container);
		if (FAILED(hr))
		{
			ReleaseConsoleOutput("gdk_save_buffer() - error: XGameSaveCreateContainer failed (HRESULT 0x%08X)\n", (unsigned)(hr));
		}

		if (SUCCEEDED(hr))
		{
			hr = XGameSaveCreateUpdate(container, container_name.c_str(), &updates);
			if (FAILED(hr))
			{
				ReleaseConsoleOutput("gdk_save_buffer() - error: XGameSaveCreateUpdate failed (HRESULT 0x%08X)\n", (unsigned)(hr));
			}
		}

		if (SUCCEEDED(hr))
		{
			for (auto b = buffers->begin(); b != buffers->end();)
			{
				hr = XGameSaveSubmitBlobWrite(updates, b->name.c_str(), (const uint8_t*)(b->data), b->data_size);
				if (FAILED(hr))
				{
					ReleaseConsoleOutput("gdk_save_buffer() - error: XGameSaveSubmitBlobWrite failed (HRESULT 0x%08X)\n", (unsigned)(hr));

					int dsMapIndex = CreateDsMap(3,
						"id", (double)(b->async_id), NULL,
						"status", (double)(false), NULL,
						"error", (double)(ConvertConnectedStorageError(hr)), NULL);
					CreateAsyncEventWithDSMap(dsMapIndex, EVENT_OTHER_ASYNC_SAVE_LOAD);

					b = buffers->erase(b);
				}
				else {
					++b;
				}
			}

			hr = ERROR_SUCCESS;
		}

		if (SUCCEEDED(hr))
		{
			hr = XGameSaveSubmitUpdate(updates);
			if (FAILED(hr))
			{
				ReleaseConsoleOutput("gdk_save_buffer() - error: XGameSaveSubmitUpdate failed (HRESULT 0x%08X)\n", (unsigned)(hr));
			}
		}

		for (auto b = buffers->begin(); b != buffers->end(); ++b)
		{
			int dsMapIndex = CreateDsMap(3,
				"id", (double)(b->async_id), NULL,
				"status", (double)(SUCCEEDED(hr) ? true : false), NULL,
				"error", (double)(ConvertConnectedStorageError(hr)), NULL);
			CreateAsyncEventWithDSMap(dsMapIndex, EVENT_OTHER_ASYNC_SAVE_LOAD);
		}

		if (updates != NULL)
		{
			XGameSaveCloseUpdate(updates);
		}

		if (container != NULL)
		{
			XGameSaveCloseContainer(container);
		}

		DecRefGameSaveProvider(gsp);
	});

	t.detach();
}

static bool _XGameSaveBlobInfoCallbackWrapper(const XGameSaveBlobInfo* info, void* context)
{
	std::function<bool(const XGameSaveBlobInfo*)>* func = (std::function<bool(const XGameSaveBlobInfo*)>*)(context);
	return (*func)(info);
}

YYEXPORT
void gdk_load_buffer(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_REAL;

	if (argc != 4)
	{
		DebugConsoleOutput("gdk_load_buffer() - error: expected 4 arguments (buffer_idx, filename, offset, length)\n");
		Result.val = -1;

		return;
	}

	int buffer_idx = YYGetInt32(arg, 0);
	const char* filename = YYGetString(arg, 1);
	int offset = YYGetInt32(arg, 2);
	int size = YYGetInt32(arg, 3);

	/* Read data from buffer just to make sure the given index actually exsits... a little wasteful
	 * but almost certainly not enough to matter.
	*/

	const unsigned char* buffer_data;
	int buffer_size;

	if(!BufferGetContent(buffer_idx, (void**)(&buffer_data), &buffer_size))
	{
		DebugConsoleOutput("gdk_load_buffer() - error: specified buffer not found\n");
		Result.val = -1;

		return;
	}

	YYFree(buffer_data);
	buffer_data = NULL;

	std::string container_name;
	std::string blob_name;
	std::tie(container_name, blob_name) = _SplitPathAndName(filename);

	int async_id = g_HTTP_ID++;

	/* Do the loading in a dedicated thread since we need to call function(s) that may
	 * block for a while (same as save path).
	*/

	std::thread t([=]()
	{
		eXboxFileError gsp_err;
		RefCountedGameSaveProvider* gsp = _SynchronousGetStorageSpace(&gsp_err);

		if (gsp == NULL)
		{
			ReleaseConsoleOutput("gdk_load_buffer() - error: unable to initialise XGameSaveProvider\n");

			int dsMapIndex = CreateDsMap(3,
				"id", (double)(async_id), NULL,
				"status", (double)(false), NULL,
				"error", (double)(gsp_err), NULL);
			CreateAsyncEventWithDSMap(dsMapIndex, EVENT_OTHER_ASYNC_SAVE_LOAD);

			return;
		}

		XGameSaveContainerHandle container = NULL;

		HRESULT hr = XGameSaveCreateContainer(gsp->provider, container_name.c_str(), &container);
		if (FAILED(hr))
		{
			ReleaseConsoleOutput("gdk_load_buffer() - error: XGameSaveCreateContainer failed (HRESULT 0x%08X)\n", (unsigned)(hr));
		}

		bool blob_found = false;
		uint32_t blobs_size;

		if (SUCCEEDED(hr))
		{
			std::function<bool(const XGameSaveBlobInfo*)> callback = [&](const XGameSaveBlobInfo* info)
			{
				/* TODO: Are these case sensitive? */
				if (strcmp(info->name, blob_name.c_str()) == 0)
				{
					blobs_size = sizeof(XGameSaveBlob) /* Structure size */
						+ strlen(info->name) + 1     /* Name + terminator */
						+ info->size;                /* Payload size */

					blob_found = true;

					return false; /* Stop enumeration. */
				}
				else {
					return true; /* Continue enumeration. */
				}
			};

			hr = XGameSaveEnumerateBlobInfoByName(container, blob_name.c_str(), &callback, &_XGameSaveBlobInfoCallbackWrapper);
			if (FAILED(hr))
			{
				ReleaseConsoleOutput("gdk_load_buffer() - error: XGameSaveEnumerateBlobInfoByName failed (HRESULT 0x%08X)\n", (unsigned)(hr));
			}
			else if (!blob_found)
			{
				ReleaseConsoleOutput("gdk_load_buffer() - error: blob '%s' not found within container '%s'\n", blob_name.c_str(), container_name.c_str());
				hr = E_GS_BLOB_NOT_FOUND;
			}
		}

		if (SUCCEEDED(hr))
		{
			const char* blob_names[] = { blob_name.c_str() };
			uint32_t num_blobs = 1;
			XGameSaveBlob* blobs = (XGameSaveBlob*)(YYAlloc(blobs_size));

			hr = XGameSaveReadBlobData(container, blob_names, &num_blobs, blobs_size, blobs);
			if (FAILED(hr))
			{
				ReleaseConsoleOutput("gdk_load_buffer() - error: XGameSaveReadBlobData failed (HRESULT 0x%08X)\n", (unsigned)(hr));
			}
			else if (num_blobs != 1)
			{
				ReleaseConsoleOutput("gdk_load_buffer() - error: Expected 1 blob, got %" PRIu32 "\n", num_blobs);
				hr = E_GS_BLOB_NOT_FOUND;
			}
			/* TODO: Case sensitive? */
			else if (strcmp(blobs->info.name, blob_name.c_str()) != 0)
			{
				ReleaseConsoleOutput("gdk_load_buffer() - error: Expected blob name '%s', got '%s'\n", blob_name.c_str(), blobs->info.name);
				hr = E_GS_BLOB_NOT_FOUND;
			}
			else {
				int to_copy = std::min<uint32_t>(size, blobs->info.size);
				int r = BufferWriteContent(buffer_idx, offset, blobs->data, to_copy, true);

				if (r >= 0)
				{
					int dsMapIndex = CreateDsMap(5,
						"id", (double)(async_id), NULL,
						"status", (double)(true), NULL,
						"error", (double)(eXboxFileError_NoError), NULL,
						"loaded_size", (double)(to_copy), NULL,
						"file_size", (double)(blobs->info.size), NULL);
					CreateAsyncEventWithDSMap(dsMapIndex, EVENT_OTHER_ASYNC_SAVE_LOAD);
				}
				else {
					/* This probably means the buffer got deleted from under us or something... */
					ReleaseConsoleOutput("gdk_load_buffer() - error: Unable to write save data to buffer\n");
					hr = E_GS_PROVIDED_BUFFER_TOO_SMALL;
				}
			}

			YYFree(blobs);
		}

		if (FAILED(hr))
		{
			int dsMapIndex = CreateDsMap(3,
				"id", (double)(async_id), NULL,
				"status", (double)(false), NULL,
				"error", (double)(ConvertConnectedStorageError(hr)), NULL);
			CreateAsyncEventWithDSMap(dsMapIndex, EVENT_OTHER_ASYNC_SAVE_LOAD);
		}

		if (container != NULL)
		{
			XGameSaveCloseContainer(container);
		}

		DecRefGameSaveProvider(gsp);
	});

	t.detach();

	Result.val = async_id;
}
