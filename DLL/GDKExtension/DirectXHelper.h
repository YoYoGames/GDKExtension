#pragma once

#include <wrl/client.h>
#include <ppl.h>
#include <ppltasks.h>

#include "GDKX.h"

#ifdef YYXBOX
#include "Files\Base\Utils.h"
#endif

#define SHOW_MESSAGE_IF_FAILED

#define DXThrowIfFailed(a,b) DX::ThrowIfFailed(##a##b, #a, __FILE__, __LINE__)
#define DXThrowIfFailedRes(a) DX::ThrowIfFailed(a, "", __FILE__, __LINE__)

namespace DX
{
	inline void ThrowIfFailed(HRESULT hr, const char* _funcstring = "", const char* _file = "", int _line = 0)
	{
		if (FAILED(hr))
		{
#ifdef SHOW_MESSAGE_IF_FAILED
			// Construct a message of the failure
			char msgtext[1024];
			const char *filestart = &_file[strlen(_file)];
			while ((filestart != _file) && (*filestart != '\\') && (*filestart != '/'))
				filestart--;
			sprintf(msgtext, "Win32 function failed: HRESULT: 0x%x\n\nCall: %s at line %d in file %s", hr, _funcstring, _line, filestart);
			ShowMessage( msgtext );
#else
			// Set a breakpoint on this line to catch Win32 API errors.
			throw Platform::Exception::CreateException(hr);
#endif
		}
	}

	// Function that reads from a binary file asynchronously.
	/*inline Concurrency::task<Platform::Array<byte>^> ReadDataAsync(Platform::String^ filename)
	{
		using namespace Windows::Storage;
		using namespace Concurrency;
		
		auto folder = Windows::ApplicationModel::Package::Current->InstalledLocation;
		
		return create_task(folder->GetFileAsync(filename)).then([] (StorageFile^ file) 
		{
			return file->OpenReadAsync();
		}).then([] (Streams::IRandomAccessStreamWithContentType^ stream)
		{
			unsigned int bufferSize = static_cast<unsigned int>(stream->Size);
			auto fileBuffer = ref new Streams::Buffer(bufferSize);
			return stream->ReadAsync(fileBuffer, bufferSize, Streams::InputStreamOptions::None);
		}).then([] (Streams::IBuffer^ fileBuffer) -> Platform::Array<byte>^ 
		{
			auto fileData = ref new Platform::Array<byte>(fileBuffer->Length);
			Streams::DataReader::FromBuffer(fileBuffer)->ReadBytes(fileData);
			return fileData;
		});
	}*/
}
