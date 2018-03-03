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
#include <ppltasks.h>
#include "MediaPlayerPlayback.h"
#include "MediaHelpers.h"

#define _Estimated1080pBitrate_ ((UINT32)(13*1000*1000))
#define _absdiff(x, y) ((x) < (y) ? (y)-(x) : (x)-(y))

#include <initguid.h>
DEFINE_GUID(DXVA_NoEncrypt, 0x1b81beD0, 0xa0c7, 0x11d3, 0xb9, 0x84, 0x00, 0xc0, 0x4f, 0x2e, 0x73, 0xc5);
DEFINE_GUID(D3D11_DECODER_PROFILE_H264_VLD_NOFGT,    0x1b81be68, 0xa0c7, 0x11d3, 0xb9, 0x84, 0x00, 0xc0, 0x4f, 0x2e, 0x73, 0xc5);

using namespace Microsoft::WRL;
using namespace ABI::Windows::Graphics::DirectX::Direct3D11;
using namespace ABI::Windows::Media;
using namespace ABI::Windows::Media::Core;
using namespace ABI::Windows::Media::Playback;
using namespace Windows::Foundation;

bool CMediaPlayerPlayback::m_deviceNotReady = true;
std::vector<CMediaPlayerPlayback*> CMediaPlayerPlayback::m_playbackObjects;
Microsoft::WRL::Wrappers::Mutex CMediaPlayerPlayback::m_playbackVectorMutex(::CreateMutex(nullptr, FALSE, nullptr));

// static method the plugin core calls when the plugin is shutting down or there is a graphics device loss 
void CMediaPlayerPlayback::GraphicsDeviceShutdown()
{
	m_deviceNotReady = true;
	auto lock = m_playbackVectorMutex.Lock();

	for (size_t i = 0; i < m_playbackObjects.size(); i++)
	{
		try
		{
			if (m_playbackObjects[i] != nullptr && !m_playbackObjects[i]->m_releasing)
				m_playbackObjects[i]->DeviceShutdown();
		}
		catch (...)
		{
		}
	}
}


// static method the plugin core calls when the plugin has been initalized or graphics device restore happened  
void CMediaPlayerPlayback::GraphicsDeviceReady(IUnityInterfaces* pUnityInterfaces)
{
	auto lock = m_playbackVectorMutex.Lock();
	IUnityGraphicsD3D11* d3d = pUnityInterfaces->Get<IUnityGraphicsD3D11>();

	if (d3d != nullptr)
	{
		m_deviceNotReady = false;
		for (size_t i = 0; i < m_playbackObjects.size(); i++)
		{
			try
			{
				if (m_playbackObjects[i] != nullptr && !m_playbackObjects[i]->m_releasing)
					m_playbackObjects[i]->DeviceReady(d3d);
			}
			catch (...)
			{
			}
		}
	}
}

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
        IFR(MakeAndInitialize<CMediaPlayerPlayback>(&spMediaPlayback, fnCallback, pClientObject, d3d));

		auto lock = m_playbackVectorMutex.Lock();
		m_playbackObjects.push_back(spMediaPlayback.Get());
	
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
	, m_fnSubtitleEntered(nullptr) 
	, m_fnSubtitleExited(nullptr)
	, m_pClientObject(nullptr)
    , m_primarySharedHandle(INVALID_HANDLE_VALUE)
	, m_uiDeviceResetToken(0)
	, m_bIgnoreEvents(false)
	, m_readyForFrames(false)
	, m_noHW4KDecoding(false)
	, m_make1080MaxWhenNoHWDecoding(true) 
	, m_releasing(false)
{
}

_Use_decl_annotations_
CMediaPlayerPlayback::~CMediaPlayerPlayback()
{
	m_releasing = true;

	auto lock = m_playbackVectorMutex.Lock();

	m_readyForFrames = false;
	m_bIgnoreEvents = true;

	ReleaseMediaPlayer();
	
	ReleaseTextures();

    ReleaseResources();

	auto position = std::find(m_playbackObjects.begin(), m_playbackObjects.end(), this);
	if (position != m_playbackObjects.end())
		(*position) = nullptr; // do not remove pointer, just make it null
}


_Use_decl_annotations_
HRESULT CMediaPlayerPlayback::RuntimeClassInitialize(
    StateChangedCallback fnCallback, 
	void* pClientObject, 
	IUnityGraphicsD3D11* pUnityDevice)
{
    Log(Log_Level_Info, L"CMediaPlayerPlayback::RuntimeClassInitialize()");

    NULL_CHK(fnCallback);
    NULL_CHK(pUnityDevice);

	m_pClientObject = pClientObject;
	m_fnStateCallback = fnCallback;

	DeviceReady(pUnityDevice);
}


_Use_decl_annotations_
HRESULT CMediaPlayerPlayback::InitializeDevices()
{
	m_d3dDevice = nullptr;
	m_mediaDevice = nullptr;

	// ref count passed in device
	ComPtr<ID3D11Device> spDevice(m_pUnityGraphics->GetDevice());

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
	// call MFUnlockDXGIDeviceManager when unloading
	if (!m_spDeviceManager)
	{
		IFR(MFLockDXGIDeviceManager(&m_uiDeviceResetToken, &m_spDeviceManager));
	}

	// associtate the device with the manager
	IFR(m_spDeviceManager->ResetDevice(spMediaDevice.Get(), m_uiDeviceResetToken));

	// create media player object
	if (!m_mediaPlayer)
	{
		IFR(CreateMediaPlayer());
	}

	m_d3dDevice.Attach(spDevice.Detach());
	m_mediaDevice.Attach(spMediaDevice.Detach());


	// check if our GPU support hardware video decoding 
	m_noHW4KDecoding = false;

	Microsoft::WRL::ComPtr<ID3D11VideoDevice> videoDevice;
	m_mediaDevice.As(&videoDevice);

	// No hardware decoding support at all? 
	if (videoDevice == nullptr)
	{
		m_noHW4KDecoding = true;
	}
	else // ok, it supports hadrware decoding in general, let's check if it supports 4K decoding 
	{
		D3D11_VIDEO_DECODER_DESC desc = { 0 };
		D3D11_VIDEO_DECODER_CONFIG config = { 0 };

		desc.Guid = D3D11_DECODER_PROFILE_H264_VLD_NOFGT;
		desc.OutputFormat = DXGI_FORMAT_NV12;
		desc.SampleHeight = 4096u;
		desc.SampleWidth = 4096u;

		config.guidConfigBitstreamEncryption = DXVA_NoEncrypt;
		config.guidConfigMBcontrolEncryption = DXVA_NoEncrypt; 
		config.guidConfigResidDiffEncryption = DXVA_NoEncrypt;
		config.ConfigBitstreamRaw = 1;
		config.ConfigResidDiffAccelerator = 1;
		config.ConfigHostInverseScan = 1;
		config.ConfigSpecificIDCT = 2;

		Microsoft::WRL::ComPtr<ID3D11VideoDecoder> videoDecoder;
		videoDevice->CreateVideoDecoder(&desc, &config, &videoDecoder);
		if (!videoDecoder)
		{
			m_noHW4KDecoding = true;
		}
	}

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
	m_readyForFrames = false;

    // create the video texture description based on texture format
	ZeroMemory(&m_textureDesc, sizeof(m_textureDesc));
    m_textureDesc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_B8G8R8A8_UNORM, width, height);
    m_textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    m_textureDesc.MipLevels = 1;
	m_textureDesc.ArraySize = 1;
	m_textureDesc.SampleDesc = { 1, 0 };
	m_textureDesc.CPUAccessFlags = 0;
	m_textureDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED; 
    m_textureDesc.Usage = D3D11_USAGE_DEFAULT;

    IFR(CreateTextures());

    ComPtr<ID3D11ShaderResourceView> spSRV;
    IFR(m_primaryTextureSRV.CopyTo(&spSRV));

    *ppvTexture = spSRV.Detach();

	m_readyForFrames = true;

    return S_OK;
}

_Use_decl_annotations_
HRESULT CMediaPlayerPlayback::LoadContent(LPCWSTR pszContentLocation)
{
    Log(Log_Level_Info, L"CMediaPlayerPlayback::LoadContent()");

	if (m_mediaPlayer.Get() == nullptr)
	{
		return E_UNEXPECTED;
	}

	// Check if MediaPlayer now has a source (Stop was not called). 
	// If so, call stop. It will recreate and reinitialize MediaPlayer (m_mediaPlayer) 
	ComPtr<IMediaPlayerSource2> spPlayerAsMediaPlayerSource;
	ComPtr<IMediaPlaybackSource> spCurrentSource;
	IFR(m_mediaPlayer.As(&spPlayerAsMediaPlayerSource));
	spPlayerAsMediaPlayerSource->get_Source(&spCurrentSource);

	if (spCurrentSource.Get())
	{
		IFR(Stop());
	}

	m_subtitleTracks.clear();

    // create the media source for content (fromUri)
    ComPtr<IMediaSource2> spMediaSource2;
	IFR(CreateMediaSource(pszContentLocation, &spMediaSource2));

	Microsoft::WRL::ComPtr<ABI::Windows::Media::Core::IMediaSource4> spMediaSource4;
	spMediaSource2.As(&spMediaSource4);
	if (spMediaSource4.Get() != nullptr)
	{
		assert(m_spAdaptiveMediaSource.Get() == nullptr);
		spMediaSource4->get_AdaptiveMediaSource(m_spAdaptiveMediaSource.ReleaseAndGetAddressOf());

		if (m_spAdaptiveMediaSource.Get() != nullptr)
		{
			OutputDebugStringW(L"We have an Adaptive Streaming media source!\n");

			// Here we make sure that we start at the highest available bitrate. 
			// If hardware video decoding for 4K+ is not supported, we limit the max bitrate to an available bitrate that is close to ~16Mbps. 
			// Remove this piece if you don't want to override the default behaviour. 
			Microsoft::WRL::ComPtr<ABI::Windows::Foundation::Collections::IVectorView<UINT32>> spAvailableBitrates;
			m_spAdaptiveMediaSource->get_AvailableBitrates(&spAvailableBitrates);
			
			if (spAvailableBitrates.Get() != nullptr)
			{
				unsigned int size = 0;
				spAvailableBitrates->get_Size(&size);

				if (size > 1)
				{
					UINT32 maxBR = 0;
					UINT32 closestTo1080BR = 0;
					UINT32 _1080Diff = UINT_MAX;

					for (unsigned int i = 0; i < size; i++)
					{
						UINT32 uBR = 0;
						spAvailableBitrates->GetAt(i, &uBR);
						if (!uBR)
							continue;

						if (uBR > maxBR)
							maxBR = uBR;

						if (_1080Diff > _absdiff(uBR, _Estimated1080pBitrate_))
						{
							closestTo1080BR = uBR;
							_1080Diff = _absdiff(uBR, _Estimated1080pBitrate_);
						}
					}

					if (!m_noHW4KDecoding && maxBR > 0)
					{
						Log(Log_Level_Any, L"Setting initial bitrate to max: %u\n", maxBR);
						m_spAdaptiveMediaSource->put_InitialBitrate(maxBR);
					}
					else if (m_noHW4KDecoding && closestTo1080BR != 0)
					{
						ComPtr<ABI::Windows::Foundation::IReference<UINT32>> spValue;
						CreateUInt32Reference(closestTo1080BR, &spValue);

						Log(Log_Level_Any, L"Setting desired max bitrate to %u\n", closestTo1080BR);
						m_spAdaptiveMediaSource->put_DesiredMaxBitrate(spValue.Get());
					}

					
				}
			}
			// end of selecting the higest available bitrate as initial or limiting the max bitrate if no HW decoding


			OutputDebugStringW(L"Adding DownloadRequested handler...");
			auto downloadRequested = Microsoft::WRL::Callback<IDownloadRequestedEventHandler>(this, &CMediaPlayerPlayback::OnDownloadRequested);
			m_spAdaptiveMediaSource->add_DownloadRequested(downloadRequested.Get(), &m_downloadRequestedEventToken);
			OutputDebugStringW(L" added.\n");
		}
	}

    IFR(CreateMediaPlaybackItem(spMediaSource2.Get(), m_spPlaybackItem.ReleaseAndGetAddressOf()));

	auto videoTracksChangedHandler = Microsoft::WRL::Callback<ITracksChangedEventHandler>(this, &CMediaPlayerPlayback::OnVideoTracksChanged);
	m_spPlaybackItem->add_VideoTracksChanged(videoTracksChangedHandler.Get(), &m_videoTracksChangedEventToken);

	auto metadataTracksChangedHandler = Microsoft::WRL::Callback<ITracksChangedEventHandler>(this, &CMediaPlayerPlayback::OnTimedMetadataTracksChanged);
	m_spPlaybackItem->add_TimedMetadataTracksChanged(metadataTracksChangedHandler.Get(), &m_timedMetadataChangedEventToken);

    ComPtr<IMediaPlaybackSource> spMediaPlaybackSource;
    IFR(m_spPlaybackItem.As(&spMediaPlaybackSource));

    IFR(spPlayerAsMediaPlayerSource->put_Source(spMediaPlaybackSource.Get()));

    return S_OK;
}

_Use_decl_annotations_
HRESULT CMediaPlayerPlayback::Play()
{
    Log(Log_Level_Info, L"CMediaPlayerPlayback::Play()");

    if (nullptr != m_mediaPlayer)
    {
        IFR(m_mediaPlayer->Play());
		return S_OK;
    }
	else
	{
		return E_ILLEGAL_METHOD_CALL;
	}
}

_Use_decl_annotations_
HRESULT CMediaPlayerPlayback::Pause()
{
    Log(Log_Level_Info, L"CMediaPlayerPlayback::Pause()");

    if (nullptr != m_mediaPlayer)
    {
        IFR(m_mediaPlayer->Pause());
		return S_OK;
    }
	else
	{
		return E_ILLEGAL_METHOD_CALL;
	}
}

_Use_decl_annotations_
HRESULT CMediaPlayerPlayback::Stop()
{
    Log(Log_Level_Info, L"CMediaPlayerPlayback::Stop()");

	bool fireStateChange = false;
	m_bIgnoreEvents = true;
	
	if (nullptr != m_mediaPlayer)
    {
		fireStateChange = true;

        ComPtr<IMediaPlayerSource2> spMediaPlayerSource;
        m_mediaPlayer.As(&spMediaPlayerSource);

		if (spMediaPlayerSource != nullptr)
		{
			spMediaPlayerSource->put_Source(nullptr);
		}

		if (m_spAdaptiveMediaSource.Get() != nullptr)
		{
			LOG_RESULT(m_spAdaptiveMediaSource->remove_DownloadRequested(m_downloadRequestedEventToken));
			m_spAdaptiveMediaSource.Reset();
			m_spAdaptiveMediaSource = nullptr;
		}

		if (m_spPlaybackItem != nullptr)
		{
			LOG_RESULT(m_spPlaybackItem->remove_TimedMetadataTracksChanged(m_timedMetadataChangedEventToken));
			LOG_RESULT(m_spPlaybackItem->remove_VideoTracksChanged(m_videoTracksChangedEventToken));
			m_spPlaybackItem.Reset();
			m_spPlaybackItem = nullptr;
		}
    }

	m_subtitleTracks.clear();

	ReleaseMediaPlayer();
	
	HRESULT hr = CreateMediaPlayer();

	if (fireStateChange)
	{
		PLAYBACK_STATE playbackState;
		ZeroMemory(&playbackState, sizeof(playbackState));
		playbackState.type = StateType::StateType_None;
		playbackState.state = PlaybackState::PlaybackState_None;

		if (m_fnStateCallback != nullptr)
			m_fnStateCallback(m_pClientObject, playbackState);
	}

	m_bIgnoreEvents = false;

	return hr;
}

_Use_decl_annotations_
HRESULT CMediaPlayerPlayback::GetDurationAndPosition(_Out_ LONGLONG* duration, _Out_ LONGLONG* position)
{
	Log(Log_Level_Info, L"CMediaPlayerPlayback::GetDurationAndPosition()"); 

	ABI::Windows::Foundation::TimeSpan durationTS;
	ABI::Windows::Foundation::TimeSpan positionTS;

	durationTS.Duration = 0;
	positionTS.Duration = 0;

	HRESULT hrPos = E_FAIL;
	HRESULT hrDur = E_FAIL;

	if (nullptr != m_mediaPlaybackSession)
	{
		if( !FAILED(m_mediaPlaybackSession->get_Position(&positionTS)) && 
			!FAILED(m_mediaPlaybackSession->get_NaturalDuration(&durationTS)))
		{
			if (duration)
			{
				*duration = durationTS.Duration;
			}

			if (position)
			{
				*position = positionTS.Duration;
			}
		}
		else
		{
			return E_FAIL;
		}
	}
	else
	{
		return E_ILLEGAL_METHOD_CALL;
	}


	return S_OK;
}

_Use_decl_annotations_
HRESULT CMediaPlayerPlayback::Seek(LONGLONG position)
{
	Log(Log_Level_Info, L"CMediaPlayerPlayback::Seek()");

	if (nullptr != m_mediaPlaybackSession)
	{
		boolean canSeek = 0;
		HRESULT hr = m_mediaPlaybackSession->get_CanSeek(&canSeek);
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
				hr = m_mediaPlaybackSession->put_Position(positionTS);
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
HRESULT CMediaPlayerPlayback::IsHardware4KDecodingSupported(_Out_ BOOL* pSupportsHardware4KVideoDecoding)
{
	Log(Log_Level_Info, L"CMediaPlayerPlayback::IsHWDecodingSupported()");

	if (!m_mediaDevice)
		return E_UNEXPECTED;

	if (!pSupportsHardware4KVideoDecoding)
		return E_INVALIDARG;

	*pSupportsHardware4KVideoDecoding = m_noHW4KDecoding ? FALSE : TRUE;

	return S_OK;
}



_Use_decl_annotations_
HRESULT CMediaPlayerPlayback::GetSubtitlesTrackCount(unsigned int * count)
{
	Log(Log_Level_Info, L"CMediaPlayerPlayback::GetSubtitlesTrackCount()");

	NULL_CHK(count);

	*count = (unsigned int)m_subtitleTracks.size();

	return S_OK;
}


_Use_decl_annotations_
HRESULT CMediaPlayerPlayback::GetSubtitlesTrack(unsigned int index, const wchar_t** trackId, const wchar_t** trackLabel, const wchar_t** trackLanguage)
{
	Log(Log_Level_Info, L"CMediaPlayerPlayback::GetSubtitlesTrack()");

	if (index >= (unsigned int)m_subtitleTracks.size())
		return E_INVALIDARG;

	*trackId = m_subtitleTracks[index].id.data();
	*trackLabel = m_subtitleTracks[index].title.data();
	*trackLanguage = m_subtitleTracks[index].language.data();

	return S_OK;
}


_Use_decl_annotations_
HRESULT CMediaPlayerPlayback::SetSubtitlesCallbacks(SubtitleItemEnteredCallback fnEnteredCallback, SubtitleItemExitedCallback fnExitedCallback)
{
	Log(Log_Level_Info, L"CMediaPlayerPlayback::SetSubtitlesCallbacks()");

	m_fnSubtitleEntered = fnEnteredCallback;
	m_fnSubtitleExited = fnExitedCallback;

	return S_OK;
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

	spMediaPlayer->put_AutoPlay(false);

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
	m_mediaPlayer = nullptr;

    m_mediaPlayer.Attach(spMediaPlayer.Detach());
    m_openedEventToken = openedEventToken;
    m_endedEventToken = endedEventToken;
    m_failedEventToken = failedEventToken;
    m_videoFrameAvailableToken = videoFrameAvailableToken;

	ComPtr<IMediaPlayer3> spMediaPlayer3;
	IFR(m_mediaPlayer.As(&spMediaPlayer3));
	m_mediaPlayer3.Attach(spMediaPlayer3.Detach());

	ComPtr<IMediaPlayer5> spMediaPlayer5;
	IFR(m_mediaPlayer.As(&spMediaPlayer5)); 
	m_mediaPlayer5.Attach(spMediaPlayer5.Detach());

	ComPtr<IMediaPlaybackSession> spSession;
	IFR(m_mediaPlayer3->get_PlaybackSession(&spSession));
	m_mediaPlaybackSession.Attach(spSession.Detach());

    IFR(AddStateChanged());

    return S_OK;
}

_Use_decl_annotations_
void CMediaPlayerPlayback::ReleaseMediaPlayer()
{
    Log(Log_Level_Info, L"CMediaPlayerPlayback::ReleaseMediaPlayer()");

	m_subtitleTracks.clear();

    RemoveStateChanged();

	m_mediaPlaybackSession.Reset();
	m_mediaPlaybackSession = nullptr;

    if (nullptr != m_mediaPlayer)
    {
		LOG_RESULT(m_mediaPlayer->remove_MediaOpened(m_openedEventToken));
		LOG_RESULT(m_mediaPlayer->remove_MediaEnded(m_endedEventToken));
		LOG_RESULT(m_mediaPlayer->remove_MediaFailed(m_failedEventToken));

		// stop playback
		ComPtr<IMediaPlayerSource2> spMediaPlayerSource;
		m_mediaPlayer.As(&spMediaPlayerSource);
		if (spMediaPlayerSource != nullptr)
			spMediaPlayerSource->put_Source(nullptr);

		if (m_spAdaptiveMediaSource.Get() != nullptr)
		{
			LOG_RESULT(m_spAdaptiveMediaSource->remove_DownloadRequested(m_downloadRequestedEventToken));
			m_spAdaptiveMediaSource.Reset();
			m_spAdaptiveMediaSource = nullptr;
		}

        if (m_mediaPlayer5)
        {
            LOG_RESULT(m_mediaPlayer5->remove_VideoFrameAvailable(m_videoFrameAvailableToken));

			m_mediaPlayer5.Reset();
			m_mediaPlayer5 = nullptr;
        }

		if (m_mediaPlayer3)
		{
			m_mediaPlayer3.Reset();
			m_mediaPlayer3 = nullptr;
		}

		if (m_spPlaybackItem != nullptr)
		{
			LOG_RESULT(m_spPlaybackItem->remove_TimedMetadataTracksChanged(m_timedMetadataChangedEventToken));
			LOG_RESULT(m_spPlaybackItem->remove_VideoTracksChanged(m_videoTracksChangedEventToken));
			m_spPlaybackItem.Reset();
			m_spPlaybackItem = nullptr;
		}

        m_mediaPlayer.Reset();
        m_mediaPlayer = nullptr;
    }
}

_Use_decl_annotations_
HRESULT CMediaPlayerPlayback::CreateTextures()
{
	Log(Log_Level_Info, L"CMediaPlayerPlayback::CreateTextures()");

    if (m_deviceNotReady ||  nullptr != m_primaryTexture || nullptr != m_primaryTextureSRV)
        ReleaseTextures();

	if (m_deviceNotReady)
		return E_ABORT;

	HRESULT hr = S_OK;

	if (!m_d3dDevice)
	{
		return E_ILLEGAL_METHOD_CALL;
	}

    // create staging texture on unity device
    ComPtr<ID3D11Texture2D> spTexture;
	IFR(m_d3dDevice->CreateTexture2D(&m_textureDesc, nullptr, spTexture.ReleaseAndGetAddressOf()));

    auto srvDesc = CD3D11_SHADER_RESOURCE_VIEW_DESC(spTexture.Get(), D3D11_SRV_DIMENSION_TEXTURE2D);
    ComPtr<ID3D11ShaderResourceView> spSRV;

	IFR(m_d3dDevice->CreateShaderResourceView(spTexture.Get(), &srvDesc, &spSRV));

    // create a shared texture from the unity texture
    ComPtr<IDXGIResource1> spDXGIResource;
    IFR(spTexture.As(&spDXGIResource));

    HANDLE sharedHandle = INVALID_HANDLE_VALUE;
    ComPtr<ID3D11Texture2D> spMediaTexture;
    ComPtr<IDirect3DSurface> spMediaSurface;

	hr = spDXGIResource->GetSharedHandle(&sharedHandle);

    if (SUCCEEDED(hr))
    {
        ComPtr<ID3D11Device1> spMediaDevice;
        hr = m_mediaDevice.As(&spMediaDevice);
        if (SUCCEEDED(hr))
        {
			hr = spMediaDevice->OpenSharedResource(sharedHandle, IID_PPV_ARGS(&spMediaTexture));

            if (SUCCEEDED(hr))
            {
                hr = GetSurfaceFromTexture(spMediaTexture.Get(), &spMediaSurface);
            }
        }
    }

	IFR(hr);

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

	m_readyForFrames = false;

    // primary texture
    if (m_primarySharedHandle != INVALID_HANDLE_VALUE)
    {
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
}


_Use_decl_annotations_
HRESULT CMediaPlayerPlayback::AddStateChanged()
{
	if (m_mediaPlaybackSession)
	{
		EventRegistrationToken stateChangedToken;
		auto stateChanged = Microsoft::WRL::Callback<IMediaPlaybackSessionEventHandler>(this, &CMediaPlayerPlayback::OnStateChanged);
		IFR(m_mediaPlaybackSession->add_PlaybackStateChanged(stateChanged.Get(), &stateChangedToken));
		m_stateChangedEventToken = stateChangedToken;

		EventRegistrationToken sizeChangedToken;
		auto sizeChanged = Microsoft::WRL::Callback<IMediaPlaybackSessionEventHandler>(this, &CMediaPlayerPlayback::OnSizeChanged);
		IFR(m_mediaPlaybackSession->add_NaturalVideoSizeChanged(sizeChanged.Get(), &sizeChangedToken));
		m_sizeChangedEventToken = sizeChangedToken;

		EventRegistrationToken durationChangedToken;
		auto durationChanged = Microsoft::WRL::Callback<IMediaPlaybackSessionEventHandler>(this, &CMediaPlayerPlayback::OnStateChanged);
		IFR(m_mediaPlaybackSession->add_NaturalDurationChanged(durationChanged.Get(), &durationChangedToken));
		m_durationChangedEventToken = durationChangedToken;
	}

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
    }
}

_Use_decl_annotations_
void CMediaPlayerPlayback::ReleaseResources()
{
	if (m_spDeviceManager != nullptr)
	{
		m_spDeviceManager = nullptr;
		MFUnlockDXGIDeviceManager();
	}

    // release dx devices
    m_mediaDevice.Reset();
    m_mediaDevice = nullptr;

    m_d3dDevice.Reset();
    m_d3dDevice = nullptr;
}

void CMediaPlayerPlayback::DeviceShutdown()
{
	m_readyForFrames = false;

	PLAYBACK_STATE playbackState;
	ZeroMemory(&playbackState, sizeof(playbackState));
	playbackState.type = StateType::StateType_GraphicsDeviceShutdown;
	playbackState.state = PlaybackState::PlaybackState_None;

	try
	{
		if (m_fnStateCallback != nullptr)
			m_fnStateCallback(m_pClientObject, playbackState);
	}
	catch (...)
	{
	}

	ReleaseTextures();
	ReleaseResources();

	m_pUnityGraphics = nullptr;
}

void CMediaPlayerPlayback::DeviceReady(IUnityGraphicsD3D11* unityD3D)
{
	m_pUnityGraphics = unityD3D;

	InitializeDevices();

	PLAYBACK_STATE playbackState;
	ZeroMemory(&playbackState, sizeof(playbackState));
	playbackState.type = StateType::StateType_GraphicsDeviceReady;
	playbackState.state = PlaybackState::PlaybackState_None;

	try
	{
		if (m_fnStateCallback != nullptr)
			m_fnStateCallback(m_pClientObject, playbackState);
	}
	catch (...)
	{
	}
}


_Use_decl_annotations_
HRESULT CMediaPlayerPlayback::OnVideoFrameAvailable(IMediaPlayer* sender, IInspectable* arg)
{
	if (!m_readyForFrames || m_deviceNotReady)
		return S_OK;

    if (nullptr != m_primaryMediaSurface && m_mediaPlayer5)
    {
		StereoscopicVideoRenderMode renderMode = StereoscopicVideoRenderMode::StereoscopicVideoRenderMode_Mono;

		if (m_mediaPlayer3 &&
			SUCCEEDED(m_mediaPlayer3->get_StereoscopicVideoRenderMode(&renderMode)) &&
			renderMode == StereoscopicVideoRenderMode::StereoscopicVideoRenderMode_Stereo)
		{
			//TODO: stereoscopic rendering with 2 textures 
		}
		else
		{
			m_mediaPlayer5->CopyFrameToVideoSurface(m_primaryMediaSurface.Get());
		}
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
	playbackState.state = PlaybackState::PlaybackState_None;
    playbackState.description.width = width;
    playbackState.description.height = height;
    playbackState.description.canSeek = canSeek;
    playbackState.description.duration = duration.Duration;

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
    playbackState.state = PlaybackState::PlaybackState_Ended;

    if (m_fnStateCallback != nullptr)
        m_fnStateCallback(m_pClientObject, playbackState);

	m_subtitleTracks.clear();

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
	playbackState.state = PlaybackState::PlaybackState_None;
    playbackState.hresult = hr;

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
    IFR(session->get_PlaybackState(&state));

    PLAYBACK_STATE playbackState;
    ZeroMemory(&playbackState, sizeof(playbackState));
    playbackState.type = StateType::StateType_StateChanged;
    playbackState.state = static_cast<PlaybackState>(state);

	if (state != MediaPlaybackState::MediaPlaybackState_None && 
		state != MediaPlaybackState::MediaPlaybackState_Opening)
	{
		// width & height of video
		UINT32 width = 0;
		UINT32 height = 0;
		if (SUCCEEDED(session->get_NaturalVideoWidth(&width)) &&
		    SUCCEEDED(session->get_NaturalVideoHeight(&height)) )
		{
			playbackState.description.width = width;
			playbackState.description.height = height;

			boolean canSeek = false;
			session->get_CanSeek(&canSeek);

			ABI::Windows::Foundation::TimeSpan duration;
			session->get_NaturalDuration(&duration);

			playbackState.description.canSeek = canSeek;
			playbackState.description.duration = duration.Duration;
		}
	}

    if (m_fnStateCallback != nullptr)
        m_fnStateCallback(m_pClientObject, playbackState);

    return S_OK;
}

_Use_decl_annotations_
HRESULT CMediaPlayerPlayback::OnSizeChanged(ABI::Windows::Media::Playback::IMediaPlaybackSession * sender, IInspectable * args)
{

	return OnStateChanged(sender, args);
}

_Use_decl_annotations_
HRESULT CMediaPlayerPlayback::OnDownloadRequested(ABI::Windows::Media::Streaming::Adaptive::IAdaptiveMediaSource * sender, ABI::Windows::Media::Streaming::Adaptive::IAdaptiveMediaSourceDownloadRequestedEventArgs * args)
{
	//ComPtr<ABI::Windows::Media::Streaming::Adaptive::IAdaptiveMediaSourceDownloadRequestedDeferral> deferral;
	//args->GetDeferral(&deferral);

	////Insert your download handling code here 

	//deferral->Complete();

	return S_OK;
}


_Use_decl_annotations_
HRESULT CMediaPlayerPlayback::OnVideoTracksChanged(IMediaPlaybackItem* pItem, ABI::Windows::Foundation::Collections::IVectorChangedEventArgs* pArgs)
{
	if (false == m_noHW4KDecoding || false == m_make1080MaxWhenNoHWDecoding || m_bIgnoreEvents)
		return S_OK;

	Log(Log_Level_Info, L"Hardware decoding of 4K+ is not supported. Selecting to find a 1080 track if found.");

	ComPtr<ABI::Windows::Foundation::Collections::IVectorView<VideoTrack*>> spVideoTracks;
	ComPtr<ABI::Windows::Media::Core::ISingleSelectMediaTrackList> spTrackList;

	if (SUCCEEDED(pItem->get_VideoTracks(&spVideoTracks)) && SUCCEEDED(spVideoTracks.As(&spTrackList)) )
	{
		ComPtr<ABI::Windows::Media::Core::IMediaTrack> track;
		ComPtr<ABI::Windows::Media::Core::IVideoTrack> vtrack;
		ComPtr<ABI::Windows::Media::MediaProperties::IVideoEncodingProperties> props;

		bool selectedATrack = false;
		INT32 selected = 0;
		unsigned int size = 0;
		unsigned int width = 0;
		unsigned int height = 0;

		spVideoTracks->get_Size(&size);

		spTrackList->get_SelectedIndex(&selected);
		if (selected >= 0)
		{
			spVideoTracks->GetAt((unsigned int)selected, track.ReleaseAndGetAddressOf());
			track.As(&vtrack);

			vtrack->GetEncodingProperties(props.ReleaseAndGetAddressOf());
			
			props->get_Width(&width);
			props->get_Height(&height);

			if (height <= 1080)
			{
				return S_OK;
			}
		}

		unsigned int maxLessThan1080Index = -1;
		unsigned int maxLessThan1080Res = -1;

		for (unsigned int i = 0; i < size; i++)
		{
			vtrack = nullptr;
			spVideoTracks->GetAt(i, track.ReleaseAndGetAddressOf());
			track.As(&vtrack);

			vtrack->GetEncodingProperties(props.ReleaseAndGetAddressOf());

			props->get_Width(&width);
			props->get_Height(&height);

			if (height < 1080 && height > maxLessThan1080Res)
			{
				maxLessThan1080Res = height;
				maxLessThan1080Index = i;
			}
			else if (height == 1080)
			{
				spTrackList->put_SelectedIndex((INT32)i);
				selectedATrack = true;
				break;
			}
		}

		if (!selectedATrack && maxLessThan1080Index != -1)
		{
			spTrackList->put_SelectedIndex(maxLessThan1080Index);
		}
	}

	return S_OK;
}


_Use_decl_annotations_
HRESULT CMediaPlayerPlayback::OnTimedMetadataTracksChanged(ABI::Windows::Media::Playback::IMediaPlaybackItem* pItem, ABI::Windows::Foundation::Collections::IVectorChangedEventArgs* pArgs)
{
	NULL_CHK(pItem);

	ComPtr<ABI::Windows::Foundation::Collections::IVectorView<ABI::Windows::Media::Core::TimedMetadataTrack*>> metadataTracks;
	ComPtr<ABI::Windows::Media::Playback::IMediaPlaybackTimedMetadataTrackList> spTrackList;
	unsigned int index = -1;
	unsigned int size = 0;
	ABI::Windows::Foundation::Collections::CollectionChange cchange = ABI::Windows::Foundation::Collections::CollectionChange::CollectionChange_Reset;

	IFR(pItem->get_TimedMetadataTracks(&metadataTracks));
	metadataTracks.As(&spTrackList);
	metadataTracks->get_Size(&size);

	m_bIgnoreEvents = true;
	
	pArgs->get_CollectionChange(&cchange);

	if (cchange == ABI::Windows::Foundation::Collections::CollectionChange::CollectionChange_ItemInserted)
	{
		pArgs->get_Index(&index);

		m_subtitleTracks.clear();
	}
	else if (cchange == ABI::Windows::Foundation::Collections::CollectionChange::CollectionChange_Reset)
	{
		m_subtitleTracks.clear();
	}
	else
	{
		size = 0;
	}

	for (unsigned int i = 0; i < size; i++)
	{
		ComPtr<ABI::Windows::Media::Core::ITimedMetadataTrack> track;

		metadataTracks->GetAt(i, track.ReleaseAndGetAddressOf());
		if (track == nullptr)
			continue;

		TimedMetadataKind kind = TimedMetadataKind::TimedMetadataKind_Custom;
		track->get_TimedMetadataKind(&kind);

		if (kind != TimedMetadataKind::TimedMetadataKind_Subtitle)
			continue;

		ComPtr<IMediaTrack> spMediaTrack;
		track.As(&spMediaTrack);

		Wrappers::HString id, label, language;
		spMediaTrack->get_Id(id.GetAddressOf());
		spMediaTrack->get_Label(label.GetAddressOf());
		spMediaTrack->get_Language(language.GetAddressOf());

		SUBTITLE_TRACK st;
		st.id = id.GetRawBuffer(nullptr);
		st.language = language.IsValid() ? language.GetRawBuffer(nullptr) : L"";
		st.title = label.IsValid() ? label.GetRawBuffer(nullptr) : L"";

		m_subtitleTracks.push_back(st);

		if (index != -1 && i != index)
			continue;

		EventRegistrationToken cueEnteredToken, cureExitedToken;
		auto spCueEnteredHandler = Microsoft::WRL::Callback<IMediaCueEventHandler>(this, &CMediaPlayerPlayback::OnCueEntered);
		auto spCueExitedHandler = Microsoft::WRL::Callback<IMediaCueEventHandler>(this, &CMediaPlayerPlayback::OnCueExited);

		track->add_CueEntered(spCueEnteredHandler.Get(), &cueEnteredToken);
		track->add_CueExited(spCueExitedHandler.Get(), &cureExitedToken);

		spTrackList->SetPresentationMode(i, ABI::Windows::Media::Playback::TimedMetadataTrackPresentationMode::TimedMetadataTrackPresentationMode_ApplicationPresented);
	}

	m_bIgnoreEvents = false;

	return S_OK;
}


_Use_decl_annotations_
HRESULT CMediaPlayerPlayback::OnCueEntered(ABI::Windows::Media::Core::ITimedMetadataTrack* pTrack, ABI::Windows::Media::Core::IMediaCueEventArgs* pArgs)
{
	ComPtr<IMediaCue> spCue;
	pArgs->get_Cue(&spCue);

	if (!spCue)
		return E_UNEXPECTED;

	ComPtr<ITimedTextCue> spTextCue;
	spCue.As(&spTextCue);

	if (!spTextCue)
		return E_UNEXPECTED;

	ComPtr<IMediaTrack> spMediaTrack;
	Wrappers::HString language, trackId;
	std::wstring langName;

	pTrack->QueryInterface(IID_IMediaTrack, &spMediaTrack);
	spMediaTrack->get_Language(language.GetAddressOf());
	if (language.IsValid())
		langName = language.GetRawBuffer(nullptr);
	spMediaTrack->get_Id(trackId.GetAddressOf());

	ComPtr<ABI::Windows::Foundation::Collections::IVector<ABI::Windows::Media::Core::TimedTextLine*>> spLines;
	spTextCue->get_Lines(&spLines);
	
	unsigned int size = 0;
	if (spLines)
		spLines->get_Size(&size);

	std::vector<const wchar_t*> lines;
	std::vector<std::wstring> linesStrings;
	const wchar_t* empty = L"";

	for (unsigned int i = 0; i < size; i++)
	{
		ComPtr<ITimedTextLine> line;
		spLines->GetAt(i, &line);

		Wrappers::HString text;

		line->get_Text(text.GetAddressOf());

		std::wstring textLine = empty;
		if (text.IsValid())
			textLine = text.GetRawBuffer(nullptr);

		linesStrings.push_back(textLine);
	}

	for (size_t i = 0; i < linesStrings.size(); i++)
	{
		lines.push_back(linesStrings[i].data());
	}

	Wrappers::HString id;
	spCue->get_Id(id.GetAddressOf());
	if (!id.IsValid())
	{
		FILETIME ft;
		GetSystemTimeAsFileTime(&ft);

		unsigned __int64 ft64 = ((unsigned __int64)(ft.dwHighDateTime) << 32) + ft.dwLowDateTime;

		wchar_t timeBuffer[32];
		memset(timeBuffer, 0, sizeof(wchar_t) * 32);
		_ui64tow_s(ft64, timeBuffer, 32, 10);
		id.Set(Wrappers::HString::MakeReference(timeBuffer, (unsigned int)wcslen(timeBuffer)).Get());

		spCue->put_Id(id.Get());
	}

	if (m_fnSubtitleEntered != nullptr)
	{
		try
		{
			m_fnSubtitleEntered(m_pClientObject, trackId.GetRawBuffer(nullptr), id.GetRawBuffer(nullptr), langName.data(), lines.data(), size);
		}
		catch (...)
		{
			Log(Log_Level_Error, L"Exception in m_fnSubtitleEntered callback");
		}
	}

	return S_OK;
}


_Use_decl_annotations_
HRESULT CMediaPlayerPlayback::OnCueExited(ABI::Windows::Media::Core::ITimedMetadataTrack* pTrack, ABI::Windows::Media::Core::IMediaCueEventArgs* pArgs)
{
	ComPtr<IMediaCue> spCue;
	pArgs->get_Cue(&spCue);

	if (!spCue)
		return E_UNEXPECTED;

	ComPtr<ITimedTextCue> spTextCue;
	spCue.As(&spTextCue);

	if (!spTextCue)
		return E_UNEXPECTED;

	ComPtr<IMediaTrack> spMediaTrack;
	pTrack->QueryInterface(IID_IMediaTrack, &spMediaTrack);

	Wrappers::HString cueId, trackId;
	spCue->get_Id(cueId.GetAddressOf());
	spMediaTrack->get_Id(trackId.GetAddressOf());

	if (m_fnSubtitleExited != nullptr)
	{
		try
		{
			m_fnSubtitleExited(m_pClientObject, trackId.GetRawBuffer(nullptr), cueId.GetRawBuffer(nullptr));
		}
		catch (...)
		{
			Log(Log_Level_Error, L"Exception in m_fnSubtitleExited callback");
		}
	}

	return S_OK;
}


