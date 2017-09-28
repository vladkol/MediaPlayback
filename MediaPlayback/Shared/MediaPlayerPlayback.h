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

#ifndef NO_FFMPEG
	#include "FFMpegInterop\FFmpegInteropMSS.h"
#endif 

#include "PlayReady\PlayReadyHandler.h"

enum class StateType : UINT32
{
    StateType_None = 0,
    StateType_Opened,
    StateType_StateChanged,
    StateType_Failed,
};

enum class PlaybackState : UINT32
{
    PlaybackState_None = 0,
    PlaybackState_Opening,
    PlaybackState_Buffering,
    PlaybackState_Playing,
    PlaybackState_Paused,
    PlaybackState_Ended
};

#pragma pack(push, 8)
typedef struct _MEDIA_DESCRIPTION
{
    UINT32 width;
    UINT32 height;
    INT64 duration;
    byte canSeek;
} MEDIA_DESCRIPTION;
#pragma pack(pop)

#pragma pack(push, 8)
typedef struct _PLAYBACK_STATE
{
    StateType type;
	PlaybackState state;
	HRESULT hresult;
	MEDIA_DESCRIPTION description;
} PLAYBACK_STATE;
#pragma pack(pop)

extern "C" typedef void(UNITY_INTERFACE_API *StateChangedCallback)(
	_In_ void* pClientObject,
    _In_ PLAYBACK_STATE args);

extern "C" typedef void(UNITY_INTERFACE_API *DRMLicenseRequestedCallback)(
	_In_ void* pClientObject);

typedef ABI::Windows::Foundation::ITypedEventHandler<ABI::Windows::Media::Playback::MediaPlayer*, IInspectable*> IMediaPlayerEventHandler;
typedef ABI::Windows::Foundation::ITypedEventHandler<ABI::Windows::Media::Playback::MediaPlayer*, ABI::Windows::Media::Playback::MediaPlayerFailedEventArgs*> IFailedEventHandler;
typedef ABI::Windows::Foundation::ITypedEventHandler<ABI::Windows::Media::Playback::MediaPlaybackSession*, IInspectable*> IMediaPlaybackSessionEventHandler;
typedef ABI::Windows::Foundation::ITypedEventHandler<ABI::Windows::Media::Streaming::Adaptive::AdaptiveMediaSource*, ABI::Windows::Media::Streaming::Adaptive::AdaptiveMediaSourceDownloadRequestedEventArgs*> IDownloadRequestedEventHandler;


DECLARE_INTERFACE_IID_(IMediaPlayerPlayback, IUnknown, "9669c78e-42c4-4178-a1e3-75b03d0f8c9a")
{
    STDMETHOD(CreatePlaybackTexture)(_In_ UINT32 width, _In_ UINT32 height, _COM_Outptr_ void** ppvTexture) PURE;
    STDMETHOD(LoadContent)(_In_ BOOL useFFmpeg, _In_ BOOL forceVideoDecode, _In_ LPCWSTR pszContentLocation) PURE;
    STDMETHOD(Play)() PURE;
    STDMETHOD(Pause)() PURE;
    STDMETHOD(Stop)() PURE;
	STDMETHOD(GetDurationAndPosition)(_Out_ LONGLONG* duration, _Out_ LONGLONG* position) PURE;
	STDMETHOD(Seek)(_In_ LONGLONG position) PURE;
	STDMETHOD(SetVolume)(_In_ DOUBLE volume) PURE;
	STDMETHOD(GetIUnknown)(_Out_ IUnknown** ppUnknown) PURE;
	STDMETHOD(SetDRMLicense)(_In_ LPCWSTR pszlicenseServiceURL, _In_ LPCWSTR pszCustomChallendgeData) PURE;
	STDMETHOD(SetDRMLicenseCallback)(_In_ DRMLicenseRequestedCallback fnCallback) PURE;
};

class CMediaPlayerPlayback
    : public Microsoft::WRL::RuntimeClass
    < Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>
    , IMediaPlayerPlayback
    , Microsoft::WRL::FtmBase>
{
public:
    static HRESULT CreateMediaPlayback(
        _In_ UnityGfxRenderer apiType, 
        _In_ IUnityInterfaces* pUnityInterfaces, 
        _In_ StateChangedCallback fnCallback,
		_In_ void* pClientObject, 
        _COM_Outptr_ IMediaPlayerPlayback** ppMediaPlayback);

    CMediaPlayerPlayback();
    ~CMediaPlayerPlayback();

    HRESULT RuntimeClassInitialize(
        _In_ StateChangedCallback fnCallback,
		_In_ void* pClientObject, 
        _In_ ID3D11Device* pDevice);

    // IMediaPlayerPlayback
    IFACEMETHOD(CreatePlaybackTexture)(
        _In_ UINT32 width, 
        _In_ UINT32 height, 
        _COM_Outptr_ void** ppvTexture);
    IFACEMETHOD(LoadContent)(
        _In_ BOOL useFFmpeg, 
        _In_ BOOL forceVideoDecode,
        _In_ LPCWSTR pszContentLocation);

    IFACEMETHOD(Play)();
    IFACEMETHOD(Pause)();
    IFACEMETHOD(Stop)();

	IFACEMETHOD(GetDurationAndPosition)(_Out_ LONGLONG* duration, _Out_ LONGLONG* position);
	IFACEMETHOD(Seek)(_In_ LONGLONG position);
	IFACEMETHOD(SetVolume)(_In_ DOUBLE volume);

	IFACEMETHOD(GetIUnknown)(_Out_ IUnknown** ppUnknown);
	IFACEMETHOD(SetDRMLicense)(_In_ LPCWSTR pszlicenseServiceURL, _In_ LPCWSTR pszCustomChallendgeData);
	IFACEMETHOD(SetDRMLicenseCallback)(_In_ DRMLicenseRequestedCallback fnCallback);

protected:
    // Callbacks - IMediaPlayer2
    HRESULT OnOpened(
        _In_ ABI::Windows::Media::Playback::IMediaPlayer* sender,
        _In_ IInspectable* args);
    HRESULT OnEnded(
        _In_ ABI::Windows::Media::Playback::IMediaPlayer* sender,
        _In_ IInspectable* args);
    HRESULT OnFailed(
        _In_ ABI::Windows::Media::Playback::IMediaPlayer* sender,
        _In_ ABI::Windows::Media::Playback::IMediaPlayerFailedEventArgs* args);

    // Callbacks - IMediaPlayer5 - frameserver
    HRESULT OnVideoFrameAvailable(
        _In_ ABI::Windows::Media::Playback::IMediaPlayer* sender,
        _In_ IInspectable* args);

    // Callbacks - IMediaPlaybackSession
    HRESULT OnStateChanged(
        _In_ ABI::Windows::Media::Playback::IMediaPlaybackSession* sender,
        _In_ IInspectable* args);

	HRESULT OnDownloadRequested(
		_In_ ABI::Windows::Media::Streaming::Adaptive::IAdaptiveMediaSource* sender,
		_In_ ABI::Windows::Media::Streaming::Adaptive::IAdaptiveMediaSourceDownloadRequestedEventArgs* args);

	static void LicenseRequestInternal(void* objectThisPtr, Microsoft::WRL::Wrappers::HString& licenseUriResult, Microsoft::WRL::Wrappers::HString& licenseCustomChallendgeDataResult);

private:
    HRESULT CreateMediaPlayer();
    void ReleaseMediaPlayer();

	HRESULT InitializeMediaPlayerWithPlayReadyDRM();

    HRESULT CreateTextures();
    void ReleaseTextures();

    HRESULT AddStateChanged();
    void RemoveStateChanged();

    void ReleaseResources();

private:
    Microsoft::WRL::ComPtr<ID3D11Device> m_d3dDevice;
    Microsoft::WRL::ComPtr<ID3D11Device> m_mediaDevice;

    StateChangedCallback m_fnStateCallback;
	DRMLicenseRequestedCallback m_fnLicenseCallback;
	void* m_pClientObject;

    Microsoft::WRL::ComPtr<ABI::Windows::Media::Playback::IMediaPlayer> m_mediaPlayer;
    EventRegistrationToken m_openedEventToken;
    EventRegistrationToken m_endedEventToken;
    EventRegistrationToken m_failedEventToken;
    EventRegistrationToken m_videoFrameAvailableToken;
	EventRegistrationToken m_downloadRequestedEventToken;

	bool m_bIgnoreEvents;
	PlayReadyHandler m_playreadyHandler;
	Microsoft::WRL::Wrappers::HString m_currentLicenseServiceURL;
	Microsoft::WRL::Wrappers::HString m_currentLicenseCustomChallendge;

#ifndef NO_FFMPEG
    Microsoft::WRL::ComPtr<FFmpegInterop::IFFmpegInteropMSS> m_ffmpegInteropMSS;
#endif

	Microsoft::WRL::ComPtr<ABI::Windows::Media::Streaming::Adaptive::IAdaptiveMediaSource> m_spAdaptiveMediaSource;

    Microsoft::WRL::ComPtr<ABI::Windows::Media::Playback::IMediaPlaybackSession> m_mediaPlaybackSession;
    EventRegistrationToken m_stateChangedEventToken;
	EventRegistrationToken m_sizeChangedEventToken;
	EventRegistrationToken m_durationChangedEventToken;

    CD3D11_TEXTURE2D_DESC m_textureDesc;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_primaryTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_primaryTextureSRV;

    HANDLE m_primarySharedHandle;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_primaryMediaTexture;
    Microsoft::WRL::ComPtr<ABI::Windows::Graphics::DirectX::Direct3D11::IDirect3DSurface> m_primaryMediaSurface;

	bool m_readyForFrames;
};

