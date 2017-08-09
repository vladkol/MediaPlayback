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
	, m_pClientObject(nullptr)
    , m_mediaPlayer(nullptr)
    , m_mediaPlaybackSession(nullptr)
    , m_primaryTexture(nullptr)
    , m_primaryTextureSRV(nullptr)
    , m_primarySharedHandle(INVALID_HANDLE_VALUE)
    , m_primaryMediaTexture(nullptr)
    , m_primaryMediaSurface(nullptr)
	, m_bIgnoreEvents(false)
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

    // create media plyaer object
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
    m_textureDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_SHARED_NTHANDLE;
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
		Microsoft::WRL::ComPtr<ABI::Windows::Media::Streaming::Adaptive::IAdaptiveMediaSource> spAdaptiveMediaSource;
		spMediaSource4->get_AdaptiveMediaSource(&spAdaptiveMediaSource);

		if (spAdaptiveMediaSource.Get() != nullptr)
		{
			OutputDebugStringW(L"Adding DownloadRequested handler...");
			EventRegistrationToken downloadRequestedEventToken;
			auto downloadRequested = Microsoft::WRL::Callback<IDownloadRequestedEventHandler>(this, &CMediaPlayerPlayback::OnDownloadRequested);
			spAdaptiveMediaSource->add_DownloadRequested(downloadRequested.Get(), &downloadRequestedEventToken);
			OutputDebugStringW(L" added.\n");
		}
	}

    ComPtr<IMediaPlaybackItem> spPlaybackItem;
    IFR(CreateMediaPlaybackItem(spMediaSource2.Get(), &spPlaybackItem));

    ComPtr<IMediaPlaybackSource> spMediaPlaybackSource;
    IFR(spPlaybackItem.As(&spMediaPlaybackSource));

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

    return S_OK;
}

_Use_decl_annotations_
HRESULT CMediaPlayerPlayback::Pause()
{
    Log(Log_Level_Info, L"CMediaPlayerPlayback::Pause()");

    if (nullptr != m_mediaPlayer)
    {
        IFR(m_mediaPlayer->Pause());
    }

    return S_OK;
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

	return S_FALSE;
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

	return S_FALSE;
}

_Use_decl_annotations_
HRESULT CMediaPlayerPlayback::SetVolume(DOUBLE volume)
{
	Log(Log_Level_Info, L"CMediaPlayerPlayback::SetVolume()");

	if (nullptr != m_mediaPlayer)
	{
		return m_mediaPlayer->put_Volume(volume);
	}

	return S_FALSE;
}


_Use_decl_annotations_
HRESULT CMediaPlayerPlayback::CreateMediaPlayer()
{
    Log(Log_Level_Info, L"CMediaPlayerPlayback::CreateMediaPlayer()");

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
    ComPtr<ID3D11Texture2D> spMediaTexture;
    ComPtr<IDirect3DSurface> spMediaSurface;

	__int64 ptr = (__int64)(void*)spDXGIResource.Get();
	WCHAR nameBuffer[MAX_PATH];
	swprintf_s(nameBuffer, MAX_PATH, L"SharedTextureHandle%I64d", ptr);

    HRESULT hr = spDXGIResource->CreateSharedHandle(
        nullptr,
        DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE,
		nameBuffer,
        &sharedHandle);
    if (SUCCEEDED(hr))
    {
        ComPtr<ID3D11Device1> spMediaDevice;
        hr = m_mediaDevice.As(&spMediaDevice);
        if (SUCCEEDED(hr))
        {
            hr = spMediaDevice->OpenSharedResource1(sharedHandle, IID_PPV_ARGS(&spMediaTexture));
            if (SUCCEEDED(hr))
            {
                hr = GetSurfaceFromTexture(spMediaTexture.Get(), &spMediaSurface);
            }
        }
    }

    // if anything failed, clean up and return
    if (FAILED(hr))
    {
        if (sharedHandle != INVALID_HANDLE_VALUE)
            CloseHandle(sharedHandle);

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
        CloseHandle(m_primarySharedHandle);
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

        m_mediaPlaybackSession.Reset();
        m_mediaPlaybackSession = nullptr;
    }
}

_Use_decl_annotations_
void CMediaPlayerPlayback::ReleaseResources()
{
    m_fnStateCallback = nullptr;

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
	
	MediaPlaybackState state;
    IFR(sender->get_PlaybackState(&state));

    PLAYBACK_STATE playbackState;
    ZeroMemory(&playbackState, sizeof(playbackState));
    playbackState.type = StateType::StateType_StateChanged;
    playbackState.value.state = static_cast<PlaybackState>(state);

    if (m_fnStateCallback != nullptr)
        m_fnStateCallback(m_pClientObject, playbackState);

    return S_OK;
}

void replaceAll(std::wstring& str, const std::wstring& from, const std::wstring& to) {
	if (from.empty())
		return;
	size_t start_pos = 0;
	while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
		str.replace(start_pos, from.length(), to);
		start_pos += to.length(); // In case 'to' contains 'from', like replacing 'x' with 'yx'
	}
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

