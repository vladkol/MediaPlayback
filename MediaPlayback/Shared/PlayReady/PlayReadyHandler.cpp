#include "pch.h" 
#include "PlayReadyHandler.h"

using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::Foundation::Collections;
using namespace Microsoft::WRL::Wrappers;

HRESULT PlayReadyHandler::InitalizeProtectionManager()
{
	Log(Log_Level_Info, L"PlayReadyHandler::InitalizeProtectionManager");

	IFR(ABI::Windows::Foundation::ActivateInstance(Wrappers::HStringReference(RuntimeClass_Windows_Media_Protection_MediaProtectionManager).Get(), &m_spProtectionManager));

	auto serviceRequested = Microsoft::WRL::Callback<IServiceRequestedEventHandler>(this, &PlayReadyHandler::OnProtectionManager_ServiceRequested);
	auto componentLoadFailed = Microsoft::WRL::Callback<IComponentLoadFailedEventHandler>(this, &PlayReadyHandler::OnProtectionManager_ComponentLoadFailed);

	IFR(m_spProtectionManager->add_ServiceRequested(
		serviceRequested.Get(),
		&m_serviceRequestToken));

	IFR(m_spProtectionManager->add_ComponentLoadFailed(
		componentLoadFailed.Get(),
		&m_componentLoadFailedToken));

	ComPtr<IPropertySet> spPropertySet;
	ComPtr<IMap<HSTRING, IInspectable*>> spMap;
	IFR(ActivateInstance(HStringReference(RuntimeClass_Windows_Foundation_Collections_PropertySet).Get(), &spPropertySet));
	IFR(spPropertySet.As(&spMap));

	IFR(AddStringProperty(spMap.Get(), L"{F4637010-03C3-42CD-B932-B48ADF3A6A54}", L"Microsoft.Media.PlayReadyClient.PlayReadyWinRTTrustedInput"));
	IFR(AddStringProperty(spMap.Get(), L"{F4637010-03C3-42CD-B932-B48ADF3A6A54}", L"Windows.Media.Protection.PlayReady.PlayReadyWinRTTrustedInput"));

	ComPtr<IPropertySet> spPMPropertySet;
	ComPtr<IMap<HSTRING, IInspectable*>> spPMPropertySetMap;
	IFR(m_spProtectionManager->get_Properties(&spPMPropertySet));
	IFR(spPMPropertySet.As(&spPMPropertySetMap));

	boolean replaced;
	IFR(spPMPropertySetMap->Insert(HStringReference(L"Windows.Media.Protection.MediaProtectionSystemIdMapping").Get(),
		spPropertySet.Get(), &replaced));
	IFR(AddStringProperty(spPMPropertySetMap.Get(), L"Windows.Media.Protection.MediaProtectionSystemId", L"{F4637010-03C3-42CD-B932-B48ADF3A6A54}"));
	IFR(AddStringProperty(spPMPropertySetMap.Get(), L"Windows.Media.Protection.MediaProtectionContainerGuid", L"{9A04F079-9840-4286-AB92-E65BE0885F95}"));
	IFR(AddBooleanProperty(spPMPropertySetMap.Get(), L"Windows.Media.Protection.UseSoftwareProtectionLayer", true));
	
	ComPtr<IPropertySet> spPMPPropertySet;
	ComPtr<IMap<HSTRING, IInspectable*>> spPMPMap;
	ComPtr<ABI::Windows::Media::Protection::IMediaProtectionPMPServerFactory> spPMPServerFactory;
	
	Windows::Foundation::ActivateInstance(Wrappers::HStringReference(RuntimeClass_Windows_Foundation_Collections_PropertySet).Get(), &spPMPPropertySet);
	IFR(spPMPPropertySet.As(&spPMPMap));
	IFR(AddStringProperty(spPMPMap.Get(), L"Windows.Media.Protection.MediaProtectionSystemId", L"{F4637010-03C3-42CD-B932-B48ADF3A6A54}"));
	IFR(AddBooleanProperty(spPMPMap.Get(), L"Windows.Media.Protection.UseSoftwareProtectionLayer", true));

	IFR(ABI::Windows::Foundation::GetActivationFactory(Wrappers::HStringReference(RuntimeClass_Windows_Media_Protection_MediaProtectionPMPServer).Get(), &spPMPServerFactory));
	IFR(spPMPServerFactory->CreatePMPServer(spPMPPropertySet.Get(), &m_spPMPServer));

	IFR(spPMPropertySetMap->Insert(HStringReference(L"Windows.Media.Protection.MediaProtectionPMPServer").Get(), m_spPMPServer.Get(), &replaced));

	return S_OK;
}

HRESULT PlayReadyHandler::OnProtectionManager_ServiceRequested(IMediaProtectionManager * sender, IServiceRequestedEventArgs * srEvent)
{
	Log(Log_Level_Info, L"PlayReadyHandler::OnProtectionManager_ServiceRequested");

	GUID aux;
	WCHAR buff[40] = { 0 };

	ComPtr<IMediaProtectionServiceRequest> spSvrRequest;
	IFR(srEvent->get_Request(&spSvrRequest));

	if (SUCCEEDED(spSvrRequest->get_Type(&aux)))
	{
		if (StringFromGUID2(aux, buff, 40))
		{
			Log(Log_Level_Info, L"License Request Type :%S", buff);
		}
	}

	return HandleServiceRequest(srEvent);
}

HRESULT PlayReadyHandler::OnProtectionManager_ComponentLoadFailed(IMediaProtectionManager * sender, IComponentLoadFailedEventArgs * e)
{
	Log(Log_Level_Info, L"PlayReadyHandler::OnProtectionManager_ComponentLoadFailed");

	HRESULT hr = S_OK;

	ComPtr<IRevocationAndRenewalInformation> spInfo;
	if (SUCCEEDED(e->get_Information(&spInfo)))
	{
		ComPtr<IVector<RevocationAndRenewalItem*>> spItems;
		unsigned int size = 0;

		hr = spInfo->get_Items(&spItems);
		if (FAILED(hr))
		{
			Log(Log_Level_Error, L"spInfo->get_Items failed: 0x%X", hr);
			return hr;
		}

		hr = spItems->get_Size(&size);
		if (FAILED(hr))
		{
			Log(Log_Level_Error, L"spItems->get_Size failed: 0x%X", hr);
			return hr;
		}

		Log(Log_Level_Info, L"Number of invalid components: %d", size);

		for (unsigned int i = 0; i < size; i++)
		{
			ComPtr<IRevocationAndRenewalItem> spRI;
			RevocationAndRenewalReasons reasons;

			if (SUCCEEDED(spItems->GetAt(i, &spRI)))
			{
				unsigned int hstrLen;
				HString  name;

				hr = spRI->get_Name(name.GetAddressOf());
				if (FAILED(hr))
				{
					return hr;
				}

				hr = spRI->get_Reasons(&reasons);
				if (FAILED(hr))
				{
					return hr;
				}

				Log(Log_Level_Info, L"Component Name: %S Reason: 0x%x", name.GetRawBuffer(&hstrLen), reasons);

			}

		}

	}

	return S_OK;
}


HRESULT PlayReadyHandler::HandleServiceRequest(IServiceRequestedEventArgs* pIevtargs)
{
	Log(Log_Level_Info, L"PlayReadyHandler::HandleServiceRequest");

	HRESULT hr = S_OK;

	ComPtr<IServiceRequestedEventArgs> spEvents;
	ComPtr<IPlayReadyServiceRequest> spPRRequest;
	ComPtr<IMediaProtectionServiceRequest> spSvrRequest;

	spEvents = pIevtargs;

	IFR(spEvents->get_Request(&spSvrRequest));

	unsigned int hstrLen;
	HString requestType;
	IFR(spSvrRequest->GetRuntimeClassName(requestType.GetAddressOf()));
	
	Log(Log_Level_Info, L"Received License Resquest, type : %S", requestType.GetRawBuffer(&hstrLen));

	GUID gType;
	spSvrRequest->get_Type(&gType);

	// Verify if is Individualization
	ComPtr<IPlayReadyIndividualizationServiceRequest> spInd;
	hr = spSvrRequest.As(&spInd);
	if (SUCCEEDED(hr))
	{
		hr = HandleIndividualizationRequest(spEvents);
	}

	if (SUCCEEDED(hr))
		return hr;

	ComPtr<IPlayReadyLicenseAcquisitionServiceRequest> spLA;

	hr = spSvrRequest.As(&spLA);
	if (SUCCEEDED(hr))
	{

		ComPtr<IPlayReadyContentHeader> spContentHeader;
		hr = spLA->get_ContentHeader(&spContentHeader);

		ComPtr<IPlayReadyServiceRequest> spSr;
		hr = spSvrRequest.As(&spSr);
		if (FAILED(hr))
		{
			Log(Log_Level_Error, L"Unable to get IPlayReadyService Request Interface");
			return hr;
		}

		return HandleLicensingRequest(spSr, spEvents);

	}

	Log(Log_Level_Error, L"Unexpected Resquest type.");

	return hr;
}


HRESULT PlayReadyHandler::HandleIndividualizationRequest(ComPtr<IServiceRequestedEventArgs> spEvents)
{
	Log(Log_Level_Info, L"PlayReadyHandler::HandleIndividualizationRequest");

	HRESULT hr = S_OK;
	unsigned int hstrLen;


	ComPtr<IPlayReadyServiceRequest> spPRRequest;
	ComPtr<IMediaProtectionServiceRequest> spSvrRequest;

	hr = spEvents->get_Request(&spSvrRequest);
	if (FAILED(hr))
	{
		return hr;
	}

	hr = spSvrRequest.As(&spPRRequest);
	if (FAILED(hr))
	{
		Log(Log_Level_Error, L"Failed to get IPlayReadyServiceRequest interface.");
		return hr;
	}

	ComPtr<IUriRuntimeClass> spUri;
	hr = spPRRequest->get_Uri(&spUri);
	if (SUCCEEDED(hr) && spUri.Get() != nullptr)
	{
		HString uri;
		hr = spUri->get_RawUri(uri.GetAddressOf());
		if (SUCCEEDED(hr))
		{
			Log(Log_Level_Info, L"Request URI: %S", uri.GetRawBuffer(&hstrLen));
		}
	}
	else
	{
		Log(Log_Level_Info, L"Unable to get request URI: 0x%X.", hr);
	}

	ComPtr<IMediaProtectionServiceCompletion> spCompletion;
	hr = spEvents->get_Completion(&spCompletion);
	if (FAILED(hr))
	{
		return hr;
	}


	auto callback = Microsoft::WRL::Callback<IAsyncActionCompletedHandler>([spCompletion](IAsyncAction* info, AsyncStatus status)
	{
		HRESULT hr;
		HRESULT result;


		result = info->GetResults();

		if (SUCCEEDED(result))
		{
			Log(Log_Level_Info, L"Completing License Individualization with success.");
			hr = spCompletion->Complete(true);
		}
		else
		{
			Log(Log_Level_Warning, L"Completing Individualization with failure: 0x%X.", result);
			hr = spCompletion->Complete(false);
		}

		if (FAILED(hr))
		{
			return hr;
		}

		return S_OK;
	});

	ComPtr<IAsyncAction> spAsyncAction;
	hr = spPRRequest->BeginServiceRequest(&spAsyncAction);
	if (FAILED(hr))
	{
		return hr;
	}

	hr = spAsyncAction->put_Completed(callback.Get());
	if (FAILED(hr))
	{
		return hr;
	}


	return hr;

}
HRESULT PlayReadyHandler::HandleLicensingRequest(ComPtr<ABI::Windows::Media::Protection::PlayReady::IPlayReadyServiceRequest> spPRRequest, ComPtr<IServiceRequestedEventArgs> spEvents)
{
	HRESULT hr = S_OK;

	Log(Log_Level_Info, L"PlayReadyHandler::HandleLicensingRequest");

	assert(m_licenseCallback);

	if (m_licenseCallback != nullptr)
	{
		Microsoft::WRL::Wrappers::HString uri;
		Microsoft::WRL::Wrappers::HString customLicenseData;

		m_licenseCallback(m_objectThis, uri, customLicenseData);

		if (uri.Get() != nullptr && WindowsGetStringLen(uri.Get()) > 0)
		{
			ComPtr<IUriRuntimeClass> spUri;
			ComPtr<IUriRuntimeClassFactory> spUriFactory;
			hr = GetActivationFactory(HStringReference(RuntimeClass_Windows_Foundation_Uri).Get(), &spUriFactory);
			if (FAILED(hr))
			{
				Log(Log_Level_Error, L"Unable to get URI factory in PlayReadyHandler::HandleLicensingRequest:0x%X", hr);
				return hr;
			}


			hr = spUriFactory->CreateUri(uri.Get(), &spUri);
			if (FAILED(hr))
			{
				Log(Log_Level_Error, L"CreateUri failed in PlayReadyHandler::HandleLicensingRequest:0x%X", hr);
				return hr;
			}

			hr = spPRRequest->put_Uri(spUri.Get());
			if (FAILED(hr))
			{
				Log(Log_Level_Error, L"put_Uri failed in PlayReadyHandler::HandleLicensingRequest:0x%X", hr);
				return hr;
			}

			if (customLicenseData != nullptr && WindowsGetStringLen(customLicenseData.Get()) > 0)
			{
				hr = spPRRequest->put_ChallengeCustomData(customLicenseData.Get());
				if (FAILED(hr))
				{
					Log(Log_Level_Error, L"put_ChallengeCustomData failed in in PlayReadyHandler::HandleLicensingRequest:0x%X", hr);
					return hr;
				}

			}
		}

	}
	else
	{
		Log(Log_Level_Warning, L"No licensing callback to call.");
	}

	ComPtr<IServiceRequestedEventArgs> spEventsToPass = spEvents;
	auto callback = Microsoft::WRL::Callback<IAsyncActionCompletedHandler>([this, spEventsToPass](IAsyncAction* info, AsyncStatus status)
	{
		return LicenseRequestCompletionCallback(spEventsToPass, info, status);
	});

	ComPtr<IAsyncAction> spAsyncAction;
	hr = spPRRequest->BeginServiceRequest(&spAsyncAction);
	if (FAILED(hr))
	{
		Log(Log_Level_Error, L"HandleLicensingRequest:BeginServiceRequest failed: 0x%X", hr);
		return hr;
	}

	hr = spAsyncAction->put_Completed(callback.Get());
	if (FAILED(hr))
	{
		Log(Log_Level_Error, L"HandleLicensingRequest:put_Completed failed: 0x%X", hr);
		return hr;
	}

	Log(Log_Level_Info, L"END OF PlayReadyHandler::HandleLicensingRequest");

	return hr;
}


HRESULT PlayReadyHandler::LicenseRequestCompletionCallback(ComPtr<IServiceRequestedEventArgs> spEvents, ABI::Windows::Foundation::IAsyncAction* info, ABI::Windows::Foundation::AsyncStatus status)
{
	HRESULT hr;
	HRESULT result;

	ComPtr<IMediaProtectionServiceCompletion> spCompletion;
	hr = spEvents->get_Completion(&spCompletion);
	if (FAILED(hr))
	{
		return hr;
	}


	result = info->GetResults();

	if (SUCCEEDED(result))
	{
		Log(Log_Level_Info, L"Completing License request (LA) with success.");
		hr = spCompletion->Complete(true);
	}
	else
	{
		Log(Log_Level_Warning, L"Completing license request (LA) with failure: 0x%X.", result);
		hr = spCompletion->Complete(false);
	}


	if (FAILED(hr))
	{
		return hr;
	}

	return S_OK;

}