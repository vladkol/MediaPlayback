#pragma once

#pragma once

#include <windows.media.h>
#include <Windows.Media.Playback.h>
#include <windows.media.protection.playready.h>
#include <string>
#include <mfidl.h>
#include <wrl/client.h>

using namespace Microsoft::WRL;
using namespace ABI::Windows::Media::Protection;
using namespace ABI::Windows::Media::Protection::PlayReady;

class PlayReadyHandler
{
public:
	PlayReadyHandler()
	{
	}

	static HRESULT HandleServiceRequest(IServiceRequestedEventArgs* pIevtargs);
};
