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

#include "pch.h"
#include "MediaPlayerPlayback.h"
#include "MediaHelpers.h"

#ifndef NO_FFMPEG
	#include "FFMpegInterop/FFmpegInteropMSS.h"
#endif

using namespace Microsoft::WRL;
using namespace ABI::Windows::Graphics::DirectX::Direct3D11;
using namespace ABI::Windows::Media;
using namespace ABI::Windows::Media::Core;
using namespace ABI::Windows::Media::Playback;
using namespace Windows::Foundation;

_Use_decl_annotations_
HRESULT CMediaPlayerPlayback::CreateMediaPlayback(
    UnityGfxRenderer apiType, 
    IUnityInterfaces* pUnityInterfaces,
    StateChangedCallback fnCallback,
	void* pClientObject, 
    IMediaPlayerPlayback** ppMediaPlayback)
{
    Log(Log_Level_Info, L"CMediaPlayerPlayback::CreateMediaPlayback()");

    NULL_CHK(pUnityInterfaces);
    NULL_CHK(fnCallback);
    NULL_CHK(ppMediaPlayback);

    *ppMediaPlayback = nullptr;

    if (apiType == kUnityGfxRendererD3D11)
    {
        IUnityGraphicsD3D11* d3d = pUnityInterfaces->Get<IUnityGraphicsD3D11>();
        NULL_CHK_HR(d3d, E_INVALIDARG);

        ComPtr<CMediaPlayerPlayback> spMediaPlayback(nullptr);
        IFR(MakeAndInitialize<CMediaPlayerPlayback>(&spMediaPlayback, fnCallback, pClientObject,d3d->GetDevice()));

        *ppMediaPlayback = spMediaPlayback.Detach();
    }
    else
    {
        IFR(E_INVALIDARG);
    }

    return S_OK;
}


_Use_decl_annotations_
CMediaPlayerPlayback::CMediaPlayerPlayback()
    : m_d3dDevice(nullptr)
    , m_mediaDevice(nullptr)
    , m_fnStateCallback(nullptr)
	, m_fnLicenseCallback(nullptr)
	, m_pClientObject(nullptr)
    , m_mediaPlayer(nullptr)
    , m_mediaPlaybackSession(nullptr)
    , m_primaryTexture(nullptr)
    , m_primaryTextureSRV(nullptr)
    , m_primarySharedHandle(INVALID_HANDLE_VALUE)
    , m_primaryMediaTexture(nullptr)
    , m_primaryMediaSurface(nullptr)
	, m_bIgnoreEvents(false)
	, m_playreadyHandler(this, CMediaPlayerPlayback::LicenseRequestInternal)
{
}

_Use_decl_annotations_
CMediaPlayerPlayback::~CMediaPlayerPlayback()
{
    ReleaseTextures();

    ReleaseMediaPlayer();

    MFUnlockDXGIDeviceManager();

    ReleaseResources();
}


_Use_decl_annotations_
HRESULT CMediaPlayerPlayback::RuntimeClassInitialize(
    StateChangedCallback fnCallback, 
	void* pClientObject, 
    ID3D11Device* pDevice)
{
    Log(Log_Level_Info, L"CMediaPlayerPlayback::RuntimeClassInitialize()");

    NULL_CHK(fnCallback);
    NULL_CHK(pDevice);

	m_pClientObject = pClientObject;

    // ref count passed in device
    ComPtr<ID3D11Device> spDevice(pDevice);

    // make sure creation of the device is on the same adapter
    ComPtr<IDXGIDevice> spDXGIDevice;
    IFR(spDevice.As(&spDXGIDevice));
    
    ComPtr<IDXGIAdapter> spAdapter;
    IFR(spDXGIDevice->GetAdapter(&spAdapter));

    // create dx device for media pipeline
    ComPtr<ID3D11Device> spMediaDevice;
    IFR(CreateMediaDevice(spAdapter.Get(), &spMediaDevice));

    // lock the shared dxgi device manager
    // will keep lock open for the life of object
    //     call MFUnlockDXGIDeviceManager when unloading
    UINT uiResetToken;
    ComPtr<IMFDXGIDeviceManager> spDeviceManager;
    IFR(MFLockDXGIDeviceManager(&uiResetToken, &spDeviceManager));

    // associtate the device with the manager
    IFR(spDeviceManager->ResetDevice(spMediaDevice.Get(), uiResetToken));

    // create media player object
    IFR(CreateMediaPlayer());

    m_fnStateCallback = fnCallback;
    m_d3dDevice.Attach(spDevice.Detach());
    m_mediaDevice.Attach(spMediaDevice.Detach());

    return S_OK;
}

_Use_decl_annotations_
HRESULT CMediaPlayerPlayback::CreatePlaybackTexture(
    UINT32 width,
    UINT32 height, 
    void** ppvTexture)
{
    NULL_CHK(ppvTexture);

    if (width < 1 || height < 1)
        IFR(E_INVALIDARG);

    *ppvTexture = nullptr;

    // create the video texture description based on texture format
    m_textureDesc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_B8G8R8A8_UNORM, width, height);
    m_textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    m_textureDesc.MipLevels = 1;
	m_textureDesc.ArraySize = 1;
	m_textureDesc.SampleDesc = { 1, 0 };
	m_textureDesc.CPUAccessFlags = 0;
	m_textureDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED; // /* commented out while investigating SHARED_NTHANDLE issue */ | D3D11_RESOURCE_MISC_SHARED_NTHANDLE;
    m_textureDesc.Usage = D3D11_USAGE_DEFAULT;

    IFR(CreateTextures());

    ComPtr<ID3D11ShaderResourceView> spSRV;
    IFR(m_primaryTextureSRV.CopyTo(&spSRV));

    *ppvTexture = spSRV.Detach();

    return S_OK;
}

_Use_decl_annotations_
HRESULT CMediaPlayerPlayback::LoadContent(
    BOOL useFFmpeg,
    BOOL forceVideoDecode, 
    LPCWSTR pszContentLocation)
{
    Log(Log_Level_Info, L"CMediaPlayerPlayback::LoadContent()");

    // create the media source for content (fromUri)
    ComPtr<IMediaSource2> spMediaSource2;
    if (useFFmpeg)
    {
#ifndef NO_FFMPEG
        // true for audio will be uncompressed
        IFR(CreateFFmpegMediaSource(pszContentLocation, true, forceVideoDecode, &m_ffmpegInteropMSS, &spMediaSource2));
#else
		OutputDebugStringW(L"\n### The library is built without FFMPEG support! ###\n");
		IFR(E_UNEXPECTED);
#endif
    }
    else
    {
        IFR(CreateMediaSource(pszContentLocation, &spMediaSource2));
    }

	Microsoft::WRL::ComPtr<ABI::Windows::Media::Core::IMediaSource4> spMediaSource4;
	spMediaSource2.As(&spMediaSource4);
	if (spMediaSource4.Get() != nullptr)
	{
		assert(m_spAdaptiveMediaSource.Get() == nullptr);
		spMediaSource4->get_AdaptiveMediaSource(m_spAdaptiveMediaSource.ReleaseAndGetAddressOf());

		if (m_spAdaptiveMediaSource.Get() != nullptr)
		{
			OutputDebugStringW(L"We have an Adaptive Streaming media source!\n");

			//Here we make sure that we start at the highest available bitrate. Remove this piece if you don't want to override the default behaviour 
			Microsoft::WRL::ComPtr<ABI::Windows::Foundation::Collections::IVectorView<UINT32>> spAvailableBitrates;
			m_spAdaptiveMediaSource->get_AvailableBitrates(&spAvailableBitrates);
			if (spAvailableBitrates.Get() != nullptr)
			{
				unsigned int size = 0;
				spAvailableBitrates->get_Size(&size);

				if (size > 1)
				{
					UINT32 maxBR = 0;
					for (unsigned int i = 0; i < size; i++)
					{
						UINT32 uBR = 0;
						spAvailableBitrates->GetAt(i, &uBR);
						if (uBR > maxBR)
							maxBR = uBR;
					}

					if (maxBR > 0)
					{
						m_spAdaptiveMediaSource->put_InitialBitrate(maxBR);
					}

					Log(Log_Level_Any, L"Setting initial bitrate to max: %u\n", maxBR);
				}
			}
			// end of selecting the higest available bitrate as initial 

			OutputDebugStringW(L"Adding DownloadRequested handler...");
			auto downloadRequested = Microsoft::WRL::Callback<IDownloadRequestedEventHandler>(this, &CMediaPlayerPlayback::OnDownloadRequested);
			m_spAdaptiveMediaSource->add_DownloadRequested(downloadRequested.Get(), &m_downloadRequestedEventToken);
			OutputDebugStringW(L" added.\n");
		}
	}

    ComPtr<IMediaPlaybackItem> spPlaybackItem;
    IFR(CreateMediaPlaybackItem(spMediaSource2.Get(), &spPlaybackItem));

    ComPtr<IMediaPlaybackSource> spMediaPlaybackSource;
    IFR(spPlaybackItem.As(&spMediaPlaybackSource));

	// Set ProtectionManager for MediaPlayer 
	IFR(InitializeMediaPlayerWithPlayReadyDRM());

    ComPtr<IMediaPlayerSource2> spMediaPlayerSource;
    IFR(m_mediaPlayer.As(&spMediaPlayerSource));
    IFR(spMediaPlayerSource->put_Source(spMediaPlaybackSource.Get()));

    return S_OK;
}

_Use_decl_annotations_
HRESULT CMediaPlayerPlayback::Play()
{
    Log(Log_Level_Info, L"CMediaPlayerPlayback::Play()");

    if (nullptr != m_mediaPlayer)
    {
        MediaPlayerState state;
        LOG_RESULT(m_mediaPlayer->get_CurrentState(&state));

        IFR(m_mediaPlayer->Play());
    }

    return E_ILLEGAL_METHOD_CALL;
}

_Use_decl_annotations_
HRESULT CMediaPlayerPlayback::Pause()
{
    Log(Log_Level_Info, L"CMediaPlayerPlayback::Pause()");

    if (nullptr != m_mediaPlayer)
    {
        IFR(m_mediaPlayer->Pause());
    }

    return E_ILLEGAL_METHOD_CALL;
}

_Use_decl_annotations_
HRESULT CMediaPlayerPlayback::Stop()
{
    Log(Log_Level_Info, L"CMediaPlayerPlayback::Stop()");

    if (nullptr != m_mediaPlayer)
    {
        ComPtr<IMediaPlayerSource2> spMediaPlayerSource;
        IFR(m_mediaPlayer.As(&spMediaPlayerSource));

		m_bIgnoreEvents = true;

		if (m_spAdaptiveMediaSource.Get() != nullptr)
		{
			LOG_RESULT(m_spAdaptiveMediaSource->remove_DownloadRequested(m_downloadRequestedEventToken));
			m_spAdaptiveMediaSource.Reset();
			m_spAdaptiveMediaSource = nullptr;
		}

        IFR(spMediaPlayerSource->put_Source(nullptr));

		m_bIgnoreEvents = false;
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT CMediaPlayerPlayback::GetDurationAndPosition(_Out_ LONGLONG* duration, _Out_ LONGLONG* position)
{
	Log(Log_Level_Info, L"CMediaPlayerPlayback::GetDurationAndPosition()"); 

	if (nullptr != m_mediaPlayer)
	{
		ComPtr<IMediaPlayer3> spMediaPlayer3;
		IFR(m_mediaPlayer.As(&spMediaPlayer3));

		ComPtr<IMediaPlaybackSession> spSession;
		IFR(spMediaPlayer3->get_PlaybackSession(&spSession));

		ABI::Windows::Foundation::TimeSpan durationTS;
		ABI::Windows::Foundation::TimeSpan positionTS;

		durationTS.Duration = 0;
		positionTS.Duration = 0;

		HRESULT hr = spSession->get_Position(&positionTS); 
		if(SUCCEEDED(hr))
			hr = spSession->get_NaturalDuration(&durationTS);

		if (duration)
		{
			*duration = durationTS.Duration;
		}

		if (position)
		{
			*position = positionTS.Duration;
		}

		return hr;
	}

	return E_ILLEGAL_METHOD_CALL;
}

_Use_decl_annotations_
HRESULT CMediaPlayerPlayback::Seek(LONGLONG position)
{
	Log(Log_Level_Info, L"CMediaPlayerPlayback::Seek()");

	if (nullptr != m_mediaPlayer)
	{
		ComPtr<IMediaPlayer3> spMediaPlayer3;
		IFR(m_mediaPlayer.As(&spMediaPlayer3));

		ComPtr<IMediaPlaybackSession> spSession;
		IFR(spMediaPlayer3->get_PlaybackSession(&spSession));

		boolean canSeek = 0;
		HRESULT hr = spSession->get_CanSeek(&canSeek);
		if (!FAILED(hr))
		{
			if (!canSeek)
			{
				hr = S_FALSE;
			}
			else
			{
				ABI::Windows::Foundation::TimeSpan positionTS;
				positionTS.Duration = position;
				hr = spSession->put_Position(positionTS);
			}
		}	
		
		return hr;
	}

	return E_ILLEGAL_METHOD_CALL;
}

_Use_decl_annotations_
HRESULT CMediaPlayerPlayback::SetVolume(DOUBLE volume)
{
	Log(Log_Level_Info, L"CMediaPlayerPlayback::SetVolume()");

	if (nullptr != m_mediaPlayer)
	{
		return m_mediaPlayer->put_Volume(volume);
	}

	return E_ILLEGAL_METHOD_CALL;
}


_Use_decl_annotations_
HRESULT CMediaPlayerPlayback::GetIUnknown(IUnknown** ppUnknown)
{
	Log(Log_Level_Info, L"CMediaPlayerPlayback::GetIUnknown()");

	HRESULT hr = E_ILLEGAL_METHOD_CALL;

	if (nullptr != m_mediaPlayer)
	{
		ComPtr<IUnknown> unkPtr;
		HRESULT hr = m_mediaPlayer.As(&unkPtr);
		if (SUCCEEDED(hr))
		{
			*ppUnknown = unkPtr.Detach();
		}
	}

	return hr;
}


_Use_decl_annotations_
HRESULT CMediaPlayerPlayback::SetDRMLicense(_In_ LPCWSTR pszlicenseServiceURL, _In_ LPCWSTR pszCustomChallendgeData)
{
	Log(Log_Level_Info, L"CMediaPlayerPlayback::SetDRMLicense()");

	Log(Log_Level_Info, L"Switching DRM License to %S (%S)",
		pszlicenseServiceURL != nullptr ? pszlicenseServiceURL : L"<nullptr>",
		pszCustomChallendgeData != nullptr ? L"has custom challendge data" : L"(no custom  challendge data)");

	if(m_currentLicenseServiceURL != nullptr)
		m_currentLicenseServiceURL.Release();
	if (m_currentLicenseCustomChallendge != nullptr)
		m_currentLicenseCustomChallendge.Release();

	if(pszlicenseServiceURL != nullptr)
		m_currentLicenseServiceURL.Set(pszlicenseServiceURL);
	
	if (pszCustomChallendgeData != nullptr)
		m_currentLicenseCustomChallendge.Set(pszCustomChallendgeData);

	return S_OK;
}


_Use_decl_annotations_
HRESULT CMediaPlayerPlayback::SetDRMLicenseCallback(_In_ DRMLicenseRequestedCallback fnCallback)
{
	Log(Log_Level_Info, L"CMediaPlayerPlayback::SetDRMLicenseCallback()");

	m_fnLicenseCallback = fnCallback;

	return S_OK;
}




_Use_decl_annotations_
HRESULT CMediaPlayerPlayback::CreateMediaPlayer()
{
    Log(Log_Level_Info, L"CMediaPlayerPlayback::CreateMediaPlayer()");

	m_playreadyHandler.InitalizeProtectionManager();

    // create media player
    ComPtr<IMediaPlayer> spMediaPlayer;
    IFR(ActivateInstance(
        Wrappers::HStringReference(RuntimeClass_Windows_Media_Playback_MediaPlayer).Get(),
        &spMediaPlayer));

    // setup callbacks
    EventRegistrationToken openedEventToken;
    auto mediaOpened = Microsoft::WRL::Callback<IMediaPlayerEventHandler>(this, &CMediaPlayerPlayback::OnOpened);
    IFR(spMediaPlayer->add_MediaOpened(mediaOpened.Get(), &openedEventToken));

    EventRegistrationToken endedEventToken;
    auto mediaEnded = Microsoft::WRL::Callback<IMediaPlayerEventHandler>(this, &CMediaPlayerPlayback::OnEnded);
    IFR(spMediaPlayer->add_MediaEnded(mediaEnded.Get(), &endedEventToken));

    EventRegistrationToken failedEventToken;
    auto mediaFailed = Microsoft::WRL::Callback<IFailedEventHandler>(this, &CMediaPlayerPlayback::OnFailed);
    IFR(spMediaPlayer->add_MediaFailed(mediaFailed.Get(), &failedEventToken));

    // frameserver mode is on the IMediaPlayer5 interface
    ComPtr<IMediaPlayer5> spMediaPlayer5;
    IFR(spMediaPlayer.As(&spMediaPlayer5));

    // set frameserver mode
    IFR(spMediaPlayer5->put_IsVideoFrameServerEnabled(true));

    // register for frame available callback
    EventRegistrationToken videoFrameAvailableToken;
    auto videoFrameAvailableCallback = Microsoft::WRL::Callback<IMediaPlayerEventHandler>(this, &CMediaPlayerPlayback::OnVideoFrameAvailable);
    IFR(spMediaPlayer5->add_VideoFrameAvailable(videoFrameAvailableCallback.Get(), &videoFrameAvailableToken));

    // store the player and token
    m_mediaPlayer.Attach(spMediaPlayer.Detach());
    m_openedEventToken = openedEventToken;
    m_endedEventToken = endedEventToken;
    m_failedEventToken = failedEventToken;
    m_videoFrameAvailableToken = videoFrameAvailableToken;

    IFR(AddStateChanged());

    return S_OK;
}

_Use_decl_annotations_
void CMediaPlayerPlayback::ReleaseMediaPlayer()
{
    Log(Log_Level_Info, L"CMediaPlayerPlayback::ReleaseMediaPlayer()");

    RemoveStateChanged();

    if (nullptr != m_mediaPlayer)
    {
		if (m_spAdaptiveMediaSource.Get() != nullptr)
		{
			LOG_RESULT(m_spAdaptiveMediaSource->remove_DownloadRequested(m_downloadRequestedEventToken));
			m_spAdaptiveMediaSource.Reset();
			m_spAdaptiveMediaSource = nullptr;
		}

        ComPtr<IMediaPlayer5> spMediaPlayer5;
        if (SUCCEEDED(m_mediaPlayer.As(&spMediaPlayer5)))
        {
            LOG_RESULT(spMediaPlayer5->remove_VideoFrameAvailable(m_videoFrameAvailableToken));

            spMediaPlayer5.Reset();
            spMediaPlayer5 = nullptr;
        }

        LOG_RESULT(m_mediaPlayer->remove_MediaOpened(m_openedEventToken));
        LOG_RESULT(m_mediaPlayer->remove_MediaEnded(m_endedEventToken));
        LOG_RESULT(m_mediaPlayer->remove_MediaFailed(m_failedEventToken));

        // stop playback
        LOG_RESULT(Stop());

        m_mediaPlayer.Reset();
        m_mediaPlayer = nullptr;
    }
}

_Use_decl_annotations_
HRESULT CMediaPlayerPlayback::CreateTextures()
{
    if (nullptr != m_primaryTexture || nullptr != m_primaryTextureSRV)
        ReleaseTextures();

    // create staging texture on unity device
    ComPtr<ID3D11Texture2D> spTexture;
    IFR(m_d3dDevice->CreateTexture2D(&m_textureDesc, nullptr, &spTexture));

    auto srvDesc = CD3D11_SHADER_RESOURCE_VIEW_DESC(spTexture.Get(), D3D11_SRV_DIMENSION_TEXTURE2D);
    ComPtr<ID3D11ShaderResourceView> spSRV;
    IFR(m_d3dDevice->CreateShaderResourceView(spTexture.Get(), &srvDesc, &spSRV));

    // create shared texture from the unity texture
    ComPtr<IDXGIResource1> spDXGIResource;
    IFR(spTexture.As(&spDXGIResource));

    HANDLE sharedHandle = INVALID_HANDLE_VALUE;
	HRESULT hr = S_OK;
    ComPtr<ID3D11Texture2D> spMediaTexture;
    ComPtr<IDirect3DSurface> spMediaSurface;

	// commented out while investigating SHARED_NTHANDLE issue
	/*__int64 ptr = (__int64)(void*)spDXGIResource.Get();
	WCHAR nameBuffer[MAX_PATH];
	swprintf_s(nameBuffer, MAX_PATH, L"SharedTextureHandle%I64d", ptr);

	
	hr = spDXGIResource->CreateSharedHandle(
        nullptr,
        DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE,
		nameBuffer,
        &sharedHandle);*/

	// added while investigating SHARED_NTHANDLE issue
	hr = spDXGIResource->GetSharedHandle(&sharedHandle);

    if (SUCCEEDED(hr))
    {
        ComPtr<ID3D11Device1> spMediaDevice;
        hr = m_mediaDevice.As(&spMediaDevice);
        if (SUCCEEDED(hr))
        {
			// OpenSharedResource1 changed to OpenSharedResource while  investigating SHARED_NTHANDLE issue
            //hr = spMediaDevice->OpenSharedResource1(sharedHandle, IID_PPV_ARGS(&spMediaTexture));
			hr = spMediaDevice->OpenSharedResource(sharedHandle, IID_PPV_ARGS(&spMediaTexture));

            if (SUCCEEDED(hr))
            {
                hr = GetSurfaceFromTexture(spMediaTexture.Get(), &spMediaSurface);
            }
        }
    }

    // if anything failed, clean up and return
    if (FAILED(hr))
    {
		// commented yout while investigating SHARED_NTHANDLE issue
        //if (sharedHandle != INVALID_HANDLE_VALUE)
        //    CloseHandle(sharedHandle);

        IFR(hr);
    }

    m_primaryTexture.Attach(spTexture.Detach());
    m_primaryTextureSRV.Attach(spSRV.Detach());

    m_primarySharedHandle = sharedHandle;
    m_primaryMediaTexture.Attach(spMediaTexture.Detach());
    m_primaryMediaSurface.Attach(spMediaSurface.Detach());

    return hr;
}

_Use_decl_annotations_
void CMediaPlayerPlayback::ReleaseTextures()
{
    Log(Log_Level_Info, L"CMediaPlayerPlayback::ReleaseTextures()");

    // primary texture
    if (m_primarySharedHandle != INVALID_HANDLE_VALUE)
    {
		// commented yout while investigating SHARED_NTHANDLE issue
        //CloseHandle(m_primarySharedHandle);
        m_primarySharedHandle = INVALID_HANDLE_VALUE;
    }

    m_primaryMediaSurface.Reset();
    m_primaryMediaSurface = nullptr;

    m_primaryMediaTexture.Reset();
    m_primaryMediaTexture = nullptr;

    m_primaryTextureSRV.Reset();
    m_primaryTextureSRV = nullptr;

    m_primaryTexture.Reset();
    m_primaryTexture = nullptr;

    ZeroMemory(&m_textureDesc, sizeof(m_textureDesc));
}

_Use_decl_annotations_
HRESULT CMediaPlayerPlayback::InitializeMediaPlayerWithPlayReadyDRM()
{
	Log(Log_Level_Info, L"CMediaPlayerPlayback::InitializeMediaPlayerWithPlayReadyDRM()");

	HRESULT hr = S_OK;

	auto pm = m_playreadyHandler.GetProtectionManager();
	if (pm.Get() == nullptr)
	{
		Log(Log_Level_Error, L"Cannot initialize protection manager.");
		return E_FAIL;
	}
	else
	{
		ComPtr<IMediaPlayerSource> spMediaPlayerSource;
		IFR(m_mediaPlayer.As(&spMediaPlayerSource));

		IFR(spMediaPlayerSource->put_ProtectionManager(pm.Get()));
	}

	return S_OK;
}


_Use_decl_annotations_
HRESULT CMediaPlayerPlayback::AddStateChanged()
{
    ComPtr<IMediaPlayer3> spMediaPlayer3;
    IFR(m_mediaPlayer.As(&spMediaPlayer3));

    ComPtr<IMediaPlaybackSession> spSession;
    IFR(spMediaPlayer3->get_PlaybackSession(&spSession));

    EventRegistrationToken stateChangedToken;
    auto stateChanged = Microsoft::WRL::Callback<IMediaPlaybackSessionEventHandler>(this, &CMediaPlayerPlayback::OnStateChanged);
    IFR(spSession->add_PlaybackStateChanged(stateChanged.Get(), &stateChangedToken));
    m_stateChangedEventToken = stateChangedToken;

	EventRegistrationToken sizeChangedToken;
	auto sizeChanged = Microsoft::WRL::Callback<IMediaPlaybackSessionEventHandler>(this, &CMediaPlayerPlayback::OnStateChanged);
	IFR(spSession->add_NaturalVideoSizeChanged(sizeChanged.Get(), &sizeChangedToken));
	m_sizeChangedEventToken = sizeChangedToken;

	EventRegistrationToken durationChangedToken;
	auto durationChanged = Microsoft::WRL::Callback<IMediaPlaybackSessionEventHandler>(this, &CMediaPlayerPlayback::OnStateChanged);
	IFR(spSession->add_NaturalDurationChanged(durationChanged.Get(), &durationChangedToken));
	m_durationChangedEventToken = durationChangedToken;


    m_mediaPlaybackSession.Attach(spSession.Detach());

    return S_OK;
}

_Use_decl_annotations_
void CMediaPlayerPlayback::RemoveStateChanged()
{
    // remove playback session callbacks
    if (nullptr != m_mediaPlaybackSession)
    {
        LOG_RESULT(m_mediaPlaybackSession->remove_PlaybackStateChanged(m_stateChangedEventToken));
		LOG_RESULT(m_mediaPlaybackSession->remove_NaturalVideoSizeChanged(m_sizeChangedEventToken));
		LOG_RESULT(m_mediaPlaybackSession->remove_NaturalDurationChanged(m_durationChangedEventToken));

        m_mediaPlaybackSession.Reset();
        m_mediaPlaybackSession = nullptr;
    }
}

_Use_decl_annotations_
void CMediaPlayerPlayback::ReleaseResources()
{
    m_fnStateCallback = nullptr;
	m_fnLicenseCallback = nullptr;

    // release dx devices
    m_mediaDevice.Reset();
    m_mediaDevice = nullptr;

    m_d3dDevice.Reset();
    m_d3dDevice = nullptr;
}

_Use_decl_annotations_
HRESULT CMediaPlayerPlayback::OnVideoFrameAvailable(IMediaPlayer* sender, IInspectable* arg)
{
    ComPtr<IMediaPlayer> spMediaPlayer(sender);

    ComPtr<IMediaPlayer5> spMediaPlayer5;
    IFR(spMediaPlayer.As(&spMediaPlayer5));

    if (nullptr != m_primaryMediaSurface)
    {
        IFR(spMediaPlayer5->CopyFrameToVideoSurface(m_primaryMediaSurface.Get()));
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT CMediaPlayerPlayback::OnOpened(
    IMediaPlayer* sender,
    IInspectable* args)
{
	if (m_bIgnoreEvents)
		return S_OK;
	
	ComPtr<IMediaPlayer> spMediaPlayer(sender);

    ComPtr<IMediaPlayer3> spMediaPlayer3;
    IFR(spMediaPlayer.As(&spMediaPlayer3));

    ComPtr<IMediaPlaybackSession> spSession;
    IFR(spMediaPlayer3->get_PlaybackSession(&spSession));

    // width & height of video
    UINT32 width = 0;
    IFR(spSession->get_NaturalVideoWidth(&width));

    UINT32 height = 0;
    IFR(spSession->get_NaturalVideoHeight(&height));

    boolean canSeek = false;
    IFR(spSession->get_CanSeek(&canSeek));

    ABI::Windows::Foundation::TimeSpan duration;
    IFR(spSession->get_NaturalDuration(&duration));

    PLAYBACK_STATE playbackState;
    ZeroMemory(&playbackState, sizeof(playbackState));
    playbackState.type = StateType::StateType_Opened;
    playbackState.value.description.width = width;
    playbackState.value.description.height = height;
    playbackState.value.description.canSeek = canSeek;
    playbackState.value.description.duration = duration.Duration;

    if (m_fnStateCallback != nullptr)
        m_fnStateCallback(m_pClientObject, playbackState);

    return S_OK;
}

_Use_decl_annotations_
HRESULT CMediaPlayerPlayback::OnEnded(
    IMediaPlayer* sender,
    IInspectable* args)
{
	if (m_bIgnoreEvents)
		return S_OK;
	
	PLAYBACK_STATE playbackState;
    ZeroMemory(&playbackState, sizeof(playbackState));
    playbackState.type = StateType::StateType_StateChanged;
    playbackState.value.state = PlaybackState::PlaybackState_Ended;

    if (m_fnStateCallback != nullptr)
        m_fnStateCallback(m_pClientObject, playbackState);

    return S_OK;
}

_Use_decl_annotations_
HRESULT CMediaPlayerPlayback::OnFailed(
    IMediaPlayer* sender, 
    IMediaPlayerFailedEventArgs* args)
{
    HRESULT hr = S_OK;

	if (m_bIgnoreEvents)
		return S_OK;

    IFR(args->get_ExtendedErrorCode(&hr));

    SafeString errorMessage;
    IFR(args->get_ErrorMessage(errorMessage.GetAddressOf()));

    LOG_RESULT_MSG(hr, errorMessage.c_str());

    PLAYBACK_STATE playbackState;
    ZeroMemory(&playbackState, sizeof(playbackState));
    playbackState.type = StateType::StateType_Failed;
    playbackState.value.hresult = hr;

    if (m_fnStateCallback != nullptr)
        m_fnStateCallback(m_pClientObject, playbackState);

    return S_OK;
}

_Use_decl_annotations_
HRESULT CMediaPlayerPlayback::OnStateChanged(
    IMediaPlaybackSession* sender,
    IInspectable* args)
{
	if (m_bIgnoreEvents)
		return S_OK;

	auto session = m_mediaPlaybackSession;
	if (session == nullptr)
		session = sender;
	
	MediaPlaybackState state;
    IFR(m_mediaPlaybackSession->get_PlaybackState(&state));

    PLAYBACK_STATE playbackState;
    ZeroMemory(&playbackState, sizeof(playbackState));
    playbackState.type = StateType::StateType_StateChanged;
    playbackState.value.state = static_cast<PlaybackState>(state);

	if (state != MediaPlaybackState::MediaPlaybackState_None && state != MediaPlaybackState::MediaPlaybackState_Opening)
	{
		// width & height of video
		UINT32 width = 0;
		IFR(session->get_NaturalVideoWidth(&width));

		UINT32 height = 0;
		IFR(session->get_NaturalVideoHeight(&height));

		boolean canSeek = false;
		IFR(session->get_CanSeek(&canSeek));

		ABI::Windows::Foundation::TimeSpan duration;
		IFR(session->get_NaturalDuration(&duration));

		playbackState.value.description.canSeek = canSeek;
		playbackState.value.description.duration = duration.Duration;
		playbackState.value.description.width = width;
		playbackState.value.description.height = height;
	}

    if (m_fnStateCallback != nullptr)
        m_fnStateCallback(m_pClientObject, playbackState);

    return S_OK;
}

_Use_decl_annotations_
HRESULT CMediaPlayerPlayback::OnDownloadRequested(ABI::Windows::Media::Streaming::Adaptive::IAdaptiveMediaSource * sender, ABI::Windows::Media::Streaming::Adaptive::IAdaptiveMediaSourceDownloadRequestedEventArgs * args)
{
	ComPtr<ABI::Windows::Media::Streaming::Adaptive::IAdaptiveMediaSourceDownloadRequestedDeferral> deferral;
	args->GetDeferral(&deferral);

#ifdef LS_HEVC_FIX
	ComPtr<ABI::Windows::Media::Streaming::Adaptive::IAdaptiveMediaSourceDownloadResult> result;
	ComPtr<ABI::Windows::Foundation::IUriRuntimeClass> uri;

	if(args->get_Result(&result) == S_OK && args->get_ResourceUri(&uri) == S_OK && uri.Get() != nullptr)
	{
		OutputDebugStringW(L"Processing resource URI...\n");
		
		Microsoft::WRL::Wrappers::HString uriString;
		uri->get_RawUri(uriString.GetAddressOf());
		
		if (uriString.Get() != nullptr)
		{
			std::wstring wuri = L"";
			wuri = uriString.GetRawBuffer(nullptr);

			OutputDebugStringW(wuri.data());

			replaceAll(wuri, L".libx265.", L".libx264.");
			replaceAll(wuri, L".hevc_nvenc.", L".h264_nvenc.");

			OutputDebugStringW(L"\n");

			Microsoft::WRL::Wrappers::HStringReference uriNewString(wuri.data());
			ComPtr<ABI::Windows::Foundation::IUriRuntimeClass> uriNew;
			ComPtr<ABI::Windows::Foundation::IUriRuntimeClassFactory> uriFactory;

			ABI::Windows::Foundation::GetActivationFactory(Microsoft::WRL::Wrappers::HStringReference(RuntimeClass_Windows_Foundation_Uri).Get(), &uriFactory);
			uriFactory->CreateUri(uriNewString.Get(), &uriNew);

			result->put_ResourceUri(uriNew.Get());
			OutputDebugStringW(L"Resource URI processed\n");
		}

	}
#endif

	deferral->Complete();

	return S_OK;
}


void CMediaPlayerPlayback::LicenseRequestInternal(void* objectThisPtr, Microsoft::WRL::Wrappers::HString& licenseUriResult, Microsoft::WRL::Wrappers::HString& licenseCustomChallendgeData)
{
	CMediaPlayerPlayback* objectThis = reinterpret_cast<CMediaPlayerPlayback*>(objectThisPtr);
	if (objectThis->m_fnLicenseCallback != nullptr)
	{
		objectThis->m_fnLicenseCallback(objectThis->m_pClientObject);

		licenseUriResult.Set(objectThis->m_currentLicenseServiceURL.Get());
		licenseCustomChallendgeData.Set(objectThis->m_currentLicenseCustomChallendge.Get());
	}
	else
	{
		Log(Log_Level_Warning, L"DRM license needed, but no licensing callback to call.");
	}
}