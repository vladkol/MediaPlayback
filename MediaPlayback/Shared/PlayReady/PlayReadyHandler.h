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

typedef void( *DRMLicenseRequestedCallbackInternal)(void* objectThis, Microsoft::WRL::Wrappers::HString& licenseUriResult, Microsoft::WRL::Wrappers::HString& licenseCustomChallendgeDataResult);


class PlayReadyHandler
{
public:
	PlayReadyHandler(void* objectThis, DRMLicenseRequestedCallbackInternal fnCallback) : 
				  m_licenseCallback(fnCallback)
				, m_objectThis(objectThis)
				, m_initialized(false)
	{
		assert(fnCallback);
		assert(objectThis);
	}


	HRESULT InitalizeProtectionManager();

	ComPtr<IMediaProtectionManager>& GetProtectionManager()
	{
		if (m_spProtectionManager == nullptr || !m_initialized)
			InitalizeProtectionManager();

		return m_spProtectionManager;
	}

private:
	HRESULT OnProtectionManager_ServiceRequested(
		IMediaProtectionManager* sender,
		IServiceRequestedEventArgs* srEvent);

	HRESULT OnProtectionManager_ComponentLoadFailed(
		IMediaProtectionManager* sender,
		IComponentLoadFailedEventArgs* e);

	HRESULT HandleIndividualizationRequest(ComPtr<IServiceRequestedEventArgs> spEvents);
	HRESULT HandleLicensingRequest(ComPtr<ABI::Windows::Media::Protection::PlayReady::IPlayReadyServiceRequest> spPRRequest, ComPtr<IServiceRequestedEventArgs> spEvents);
	HRESULT HandleServiceRequest(IServiceRequestedEventArgs* pIevtargs);
	HRESULT LicenseRequestCompletionCallback(ComPtr<IServiceRequestedEventArgs> spEvents, ABI::Windows::Foundation::IAsyncAction* info, ABI::Windows::Foundation::AsyncStatus status);

private:
	ComPtr<IMediaProtectionManager> m_spProtectionManager;
	ComPtr<IMediaProtectionPMPServer> m_spPMPServer;
	EventRegistrationToken m_serviceRequestToken;
	EventRegistrationToken m_componentLoadFailedToken;
	DRMLicenseRequestedCallbackInternal m_licenseCallback;
	void* m_objectThis;
	bool m_initialized;
};
