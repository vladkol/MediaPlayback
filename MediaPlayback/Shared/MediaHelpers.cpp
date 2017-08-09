//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#pragma once

#include "pch.h"
#include "MediaHelpers.h"

using namespace ABI::Windows::Graphics::DirectX::Direct3D11;
using namespace ABI::Windows::Media::Core;
using namespace ABI::Windows::Media::Playback;
using namespace ABI::Windows::Media::Streaming::Adaptive;

using namespace Microsoft::WRL;

#if defined(_DEBUG)
// Check for SDK Layer support.
inline bool SdkLayersAvailable()
{
    HRESULT hr = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_NULL,       // There is no need to create a real hardware device.
        0,
        D3D11_CREATE_DEVICE_DEBUG,  // Check for the SDK layers.
        nullptr,                    // Any feature level will do.
        0,
        D3D11_SDK_VERSION,          // Always set this to D3D11_SDK_VERSION for Windows Store apps.
        nullptr,                    // No need to keep the D3D device reference.
        nullptr,                    // No need to know the feature level.
        nullptr                     // No need to keep the D3D device context reference.
    );

    return SUCCEEDED(hr);
}
#endif

using namespace ABI::Windows::Media::Streaming::Adaptive;
using namespace ABI::Windows::Foundation;
using namespace Microsoft::WRL::Wrappers;

#include <ppl.h>
#include <ppltasks.h>

void CreateAdaptiveMediaSourceFromUri(
	_In_ PCWSTR szManifestUri,
	_Outptr_opt_ IAdaptiveMediaSource** ppAdaptiveMediaSource,
	_Outptr_opt_ IAdaptiveMediaSourceCreationResult** ppCreationResult
)
{
	HRESULT hr = S_OK;

	if (ppAdaptiveMediaSource != nullptr)
	{
		*ppAdaptiveMediaSource = nullptr;
	}

	if (ppCreationResult != nullptr)
	{
		*ppCreationResult = nullptr;
	}

	ComPtr<IUriRuntimeClassFactory> spUriFactory;
	ABI::Windows::Foundation::GetActivationFactory(
		Microsoft::WRL::Wrappers::HStringReference(RuntimeClass_Windows_Foundation_Uri).Get(),
		&spUriFactory);

	ComPtr<IUriRuntimeClass> spUri;
	spUriFactory->CreateUri(
		HStringReference(szManifestUri).Get(),
		&spUri
	);

	ComPtr<IAdaptiveMediaSourceStatics> spMediaSourceStatics;
	Windows::Foundation::GetActivationFactory(
		HStringReference(RuntimeClass_Windows_Media_Streaming_Adaptive_AdaptiveMediaSource).Get(),
		&spMediaSourceStatics
	);

	ComPtr<ICreateAdaptiveMediaSourceOperation> spCreateOperation;
	Event operationCompleted(CreateEvent(nullptr, TRUE, FALSE, nullptr));

	HRESULT hrStatus = S_OK;
	ComPtr<IAdaptiveMediaSourceCreationResult> spResult;

	auto callback = Callback<ABI::Windows::Foundation::IAsyncOperationCompletedHandler<AdaptiveMediaSourceCreationResult*>>(
		[&operationCompleted, &hrStatus, &spResult](
			ICreateAdaptiveMediaSourceOperation *pOp,
			AsyncStatus status
			) -> HRESULT
	{
		if (status == AsyncStatus::Completed)
		{
			hrStatus = pOp->GetResults(&spResult);
		}
		else
		{
			hrStatus = E_FAIL;
		}

		SetEvent(operationCompleted.Get());
		return S_OK;
	});

	concurrency::create_task([&] (){
		spMediaSourceStatics->CreateFromUriAsync(
			spUri.Get(),
			&spCreateOperation
		);
		if (spCreateOperation)
		{
			spCreateOperation->put_Completed(callback.Get());
		}
		else
		{
			SetEvent(operationCompleted.Get());
		}
	});


	DWORD result = WaitForSingleObject(operationCompleted.Get(), INFINITE);

	AdaptiveMediaSourceCreationStatus creationStatus = AdaptiveMediaSourceCreationStatus_UnknownFailure;
	if(spResult)
		spResult->get_Status(&creationStatus);

	if (creationStatus == AdaptiveMediaSourceCreationStatus_Success)
	{
		ComPtr<IAdaptiveMediaSource> spMediaSource;
		spResult->get_MediaSource(&spMediaSource);

		if (ppAdaptiveMediaSource != nullptr)
		{
			*ppAdaptiveMediaSource = spMediaSource.Detach();
		}
	}

	if (ppCreationResult != nullptr)
	{
		*ppCreationResult = spResult.Detach();
	}
}

_Use_decl_annotations_
HRESULT CreateMediaSource(
    LPCWSTR pszUrl,
    IMediaSource2** ppMediaSource)
{
    NULL_CHK(pszUrl);
    NULL_CHK(ppMediaSource);

    *ppMediaSource = nullptr;

    // convert the uri
    Microsoft::WRL::ComPtr<ABI::Windows::Foundation::IUriRuntimeClassFactory> spUriFactory;
    IFR(ABI::Windows::Foundation::GetActivationFactory(
        Microsoft::WRL::Wrappers::HStringReference(RuntimeClass_Windows_Foundation_Uri).Get(),
        &spUriFactory));

    Microsoft::WRL::ComPtr<ABI::Windows::Foundation::IUriRuntimeClass> spUri;
    IFR(spUriFactory->CreateUri(
        Microsoft::WRL::Wrappers::HStringReference(pszUrl).Get(),
        &spUri));

    // create a media source
    Microsoft::WRL::ComPtr<ABI::Windows::Media::Core::IMediaSourceStatics> spMediaSourceStatics;
    IFR(ABI::Windows::Foundation::GetActivationFactory(
        Microsoft::WRL::Wrappers::HStringReference(RuntimeClass_Windows_Media_Core_MediaSource).Get(),
        &spMediaSourceStatics));

	Microsoft::WRL::ComPtr<ABI::Windows::Media::Core::IMediaSource2> spMediaSource2;

	Microsoft::WRL::ComPtr<ABI::Windows::Media::Streaming::Adaptive::IAdaptiveMediaSource> adaptiveSource;
	CreateAdaptiveMediaSourceFromUri(pszUrl, adaptiveSource.GetAddressOf(), nullptr);
	
	if(adaptiveSource != nullptr)
		spMediaSourceStatics->CreateFromAdaptiveMediaSource(adaptiveSource.Get(), &spMediaSource2);
	
	if(spMediaSource2.Get() == nullptr)
	{
		IFR(spMediaSourceStatics->CreateFromUri(
			spUri.Get(),
			&spMediaSource2));
	}


    *ppMediaSource = spMediaSource2.Detach();

    return S_OK;
}

HRESULT CreateFFmpegMediaSource(
    _In_ LPCWSTR pszUrl,
    _In_ bool forceAudioDecode,
    _In_ bool forceVideoDecode,
    _COM_Outptr_ FFmpegInterop::IFFmpegInteropMSS** ppFFmpegMSS,
    _COM_Outptr_ IMediaSource2** ppMediaSource)
{
    NULL_CHK(pszUrl);
    NULL_CHK(ppMediaSource);

    *ppMediaSource = nullptr;

    // Set FFmpeg specific options. List of options can be found in https://www.ffmpeg.org/ffmpeg-protocols.html
    ComPtr<ABI::Windows::Foundation::Collections::IPropertySet> spPropertySet;

    // if you need to set additional properties for FFmpeg, this would allow you to do that here
#if 0 
    {
        hr = ABI::Windows::Foundation::ActivateInstance(Wrappers::HStringReference(RuntimeClass_Windows_Foundation_Collections_PropertySet).Get(), &spPropertySet);
        if (FAILED(hr))
            return hr;

        // Below are some sample options that you can set to configure RTSP streaming
        // options->Insert("rtsp_flags", "prefer_tcp");
        // options->Insert("stimeout", 100000);
        ComPtr<ABI::Windows::Foundation::Collections::IMap<HSTRING, IInspectable*>> spPropertyMap;
        hr = spPropertySet.As(&spPropertyMap);
        if (FAILED(hr))
            return hr;

        Wrappers::HStringReference key(L"stimeout");
        Wrappers::HStringReference value(L"100000");

        ComPtr<IActivationFactory> spGenericActivationFactoryInterface;
        hr = Windows::Foundation::GetActivationFactory(Wrappers::HStringReference(RuntimeClass_Windows_Foundation_PropertyValue).Get(), &spGenericActivationFactoryInterface);
        if (FAILED(hr))
            return hr;

        ComPtr<ABI::Windows::Foundation::IPropertyValueStatics> spFactory;
        hr = spGenericActivationFactoryInterface.As(&spFactory);
        if (FAILED(hr))
            return hr;

        ComPtr<IInspectable> inspectableValue;
        hr = spFactory->CreateString(value.Get(), &inspectableValue);
        if (FAILED(hr))
            return hr;

        boolean replaced = false;
        hr = spPropertyMap->Insert(key.Get(), inspectableValue.Get(), &replaced);
        if (FAILED(hr))
            return hr;
    }
#endif

    ComPtr<FFmpegInterop::IFFmpegInteropMSS> interopMSS;
    IFR(MakeAndInitialize<FFmpegInterop::FFmpegInteropMSS>(&interopMSS, pszUrl, forceAudioDecode, forceVideoDecode, spPropertySet.Get()));

    ComPtr<IMediaStreamSource> spMediaStreamSource;
    IFR(interopMSS->get_MediaStreamSource(&spMediaStreamSource));

    ComPtr<IMediaSourceStatics> spMediaSourceStatics;
    IFR(ABI::Windows::Foundation::GetActivationFactory(
        Wrappers::HStringReference(RuntimeClass_Windows_Media_Core_MediaSource).Get(),
        &spMediaSourceStatics));

    ComPtr<IMediaSource2> spMediaSource2;
    IFR(spMediaSourceStatics->CreateFromMediaStreamSource(
        spMediaStreamSource.Get(),
        &spMediaSource2));

    *ppFFmpegMSS = interopMSS.Detach();
    *ppMediaSource = spMediaSource2.Detach();

    return S_OK;
}

HRESULT CreateAdaptiveMediaSource(
    LPCWSTR pszManifestLocation,
    IAdaptiveMediaSourceCompletedCallback* pCallback)
{
    NULL_CHK(pszManifestLocation);
    NULL_CHK(pCallback);

    ComPtr<IAdaptiveMediaSourceCompletedCallback> spCallback(pCallback);

    // convert the uri
    ComPtr<ABI::Windows::Foundation::IUriRuntimeClassFactory> spUriFactory;
    IFR(ABI::Windows::Foundation::GetActivationFactory(
        Wrappers::HStringReference(RuntimeClass_Windows_Foundation_Uri).Get(),
        &spUriFactory));

    ComPtr<ABI::Windows::Foundation::IUriRuntimeClass> spUri;
    IFR(spUriFactory->CreateUri(
        Wrappers::HStringReference(pszManifestLocation).Get(),
        &spUri));

    // get factory for adaptive source
    ComPtr<IAdaptiveMediaSourceStatics> spAdaptiveSourceStatics;
    IFR(ABI::Windows::Foundation::GetActivationFactory(
        Wrappers::HStringReference(RuntimeClass_Windows_Media_Streaming_Adaptive_AdaptiveMediaSource).Get(),
        &spAdaptiveSourceStatics));

    // get the asyncOp for creating the source
    ComPtr<ICreateAdaptiveMediaSourceOperation> asyncOp;
    IFR(spAdaptiveSourceStatics->CreateFromUriAsync(spUri.Get(), &asyncOp));

    // create a completed callback
    auto completedHandler = Microsoft::WRL::Callback<ICreateAdaptiveMediaSourceResultHandler>(
        [spCallback, asyncOp](_In_ ICreateAdaptiveMediaSourceOperation *pOp, _In_ AsyncStatus status) -> HRESULT
    {
        return spCallback->OnAdaptiveMediaSourceCreated(pOp, status);
    });

    IFR(asyncOp->put_Completed(completedHandler.Get()));

    return S_OK;
}

_Use_decl_annotations_
HRESULT CreateMediaPlaybackItem(
    _In_ IMediaSource2* pMediaSource,
    _COM_Outptr_ IMediaPlaybackItem** ppMediaPlaybackItem)
{
    // create playbackitem from source
    ComPtr<IMediaPlaybackItemFactory> spItemFactory;
    IFR(ABI::Windows::Foundation::GetActivationFactory(
        Wrappers::HStringReference(RuntimeClass_Windows_Media_Playback_MediaPlaybackItem).Get(),
        &spItemFactory));

    ComPtr<IMediaPlaybackItem> spItem;
    IFR(spItemFactory->Create(pMediaSource, &spItem));

    *ppMediaPlaybackItem = spItem.Detach();

    return S_OK;
}

_Use_decl_annotations_
HRESULT CreatePlaylistSource(
    IMediaSource2* pSource,
    IMediaPlaybackSource** ppMediaPlaybackSource)
{
    NULL_CHK(pSource);
    NULL_CHK(ppMediaPlaybackSource);

    *ppMediaPlaybackSource = nullptr;

    ComPtr<IMediaPlaybackList> spPlaylist;
    IFR(Windows::Foundation::ActivateInstance(
        Wrappers::HStringReference(RuntimeClass_Windows_Media_Playback_MediaPlaybackList).Get(),
        &spPlaylist));

    // get the iterator for playlist
    ComPtr<ABI::Windows::Foundation::Collections::IObservableVector<MediaPlaybackItem*>> spItems;
    IFR(spPlaylist->get_Items(&spItems));

    ComPtr<ABI::Windows::Foundation::Collections::IVector<MediaPlaybackItem*>> spItemsVector;
    IFR(spItems.As(&spItemsVector));

    // create playbackitem from source
    ComPtr<IMediaPlaybackItem> spItem;
    IFR(CreateMediaPlaybackItem(pSource, &spItem));

    // add to the list
    IFR(spItemsVector->Append(spItem.Get()));

    // convert to playbackSource
    ComPtr<IMediaPlaybackSource> spMediaPlaybackSource;
    IFR(spPlaylist.As(&spMediaPlaybackSource));

    *ppMediaPlaybackSource = spMediaPlaybackSource.Detach();

    return S_OK;
}

_Use_decl_annotations_
HRESULT GetSurfaceFromTexture(
    ID3D11Texture2D* pTexture,
    IDirect3DSurface** ppSurface)
{
    NULL_CHK(pTexture);
    NULL_CHK(ppSurface);

    *ppSurface = nullptr;

    ComPtr<ID3D11Texture2D> spTexture(pTexture);

    ComPtr<IDXGISurface> dxgiSurface;
    IFR(spTexture.As(&dxgiSurface));

    ComPtr<IInspectable> inspectableSurface;
    IFR(CreateDirect3D11SurfaceFromDXGISurface(dxgiSurface.Get(), &inspectableSurface));

    ComPtr<IDirect3DSurface> spSurface;
    IFR(inspectableSurface.As(&spSurface));

    *ppSurface = spSurface.Detach();

    return S_OK;
}

_Use_decl_annotations_
HRESULT GetTextureFromSurface(
    IDirect3DSurface* pSurface,
    ID3D11Texture2D** ppTexture)
{
    NULL_CHK(pSurface);
    NULL_CHK(ppTexture);

    *ppTexture = nullptr;

    ComPtr<Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess> spDXGIInterfaceAccess;
    IFR(pSurface->QueryInterface(IID_PPV_ARGS(&spDXGIInterfaceAccess)));
    IFR(spDXGIInterfaceAccess->GetInterface(IID_PPV_ARGS(ppTexture)));

    return S_OK;
}

_Use_decl_annotations_
HRESULT CreateMediaDevice(
    IDXGIAdapter* pDXGIAdapter, 
    ID3D11Device** ppDevice)
{
    NULL_CHK(ppDevice);

    // Create the Direct3D 11 API device object and a corresponding context.
    D3D_FEATURE_LEVEL featureLevel;

    // This flag adds support for surfaces with a different color channel ordering
    // than the API default. It is required for compatibility with Direct2D.
    UINT creationFlags = D3D11_CREATE_DEVICE_VIDEO_SUPPORT | D3D11_CREATE_DEVICE_BGRA_SUPPORT;

#if defined(_DEBUG)
    if (SdkLayersAvailable())
    {
        // If the project is in a debug build, enable debugging via SDK Layers with this flag.
        creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
    }
#endif
    D3D_FEATURE_LEVEL featureLevels[] =
    {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };

    ComPtr<ID3D11Device> spDevice;
    ComPtr<ID3D11DeviceContext> spContext;

    D3D_DRIVER_TYPE driverType = (nullptr != pDXGIAdapter) ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE;

    // Create a device using the hardware graphics driver if adapter is not supplied
    HRESULT hr = D3D11CreateDevice(
        pDXGIAdapter,               // if nullptr will use default adapter.
        driverType, 
        0,                          // Should be 0 unless the driver is D3D_DRIVER_TYPE_SOFTWARE.
        creationFlags,              // Set debug and Direct2D compatibility flags.
        featureLevels,              // List of feature levels this app can support.
        ARRAYSIZE(featureLevels),   // Size of the list above.
        D3D11_SDK_VERSION,          // Always set this to D3D11_SDK_VERSION for Windows Store apps.
        &spDevice,                  // Returns the Direct3D device created.
        &featureLevel,              // Returns feature level of device created.
        &spContext                  // Returns the device immediate context.
    );

    if (FAILED(hr))
    {
        // fallback to WARP if we are not specifying an adapter
        if (nullptr == pDXGIAdapter)
        {
            // If the initialization fails, fall back to the WARP device.
            // For more information on WARP, see: 
            // http://go.microsoft.com/fwlink/?LinkId=286690
            hr = D3D11CreateDevice(
                nullptr,
                D3D_DRIVER_TYPE_WARP, // Create a WARP device instead of a hardware device.
                0,
                creationFlags,
                featureLevels,
                ARRAYSIZE(featureLevels),
                D3D11_SDK_VERSION,
                &spDevice,
                &featureLevel,
                &spContext);
        }

        IFR(hr);
    }
    else
    {
        // workaround for nvidia GPU's, cast to ID3D11VideoDevice
        Microsoft::WRL::ComPtr<ID3D11VideoDevice> videoDevice;
        spDevice.As(&videoDevice);
    }

    // Turn multithreading on 
    ComPtr<ID3D10Multithread> spMultithread;
    if (SUCCEEDED(spContext.As(&spMultithread)))
    {
        spMultithread->SetMultithreadProtected(TRUE);
    }

    *ppDevice = spDevice.Detach();

    return S_OK;
}
