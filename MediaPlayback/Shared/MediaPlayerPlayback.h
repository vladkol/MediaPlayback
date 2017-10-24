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

#include <vector>
#include <string>

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
	StateType_DeviceLost
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

typedef struct _SUBTITLE_TRACK
{
	std::wstring id;
	std::wstring title;
	std::wstring language;
} SUBTITLE_TRACK;

extern "C" typedef void(UNITY_INTERFACE_API *StateChangedCallback)(
	_In_ void* pClientObject,
    _In_ PLAYBACK_STATE args);

extern "C" typedef void(UNITY_INTERFACE_API *DRMLicenseRequestedCallback)(
	_In_ void* pClientObject);

extern "C" typedef void(UNITY_INTERFACE_API *SubtitleItemEnteredCallback)(
	_In_ void* pClientObject, const wchar_t* subtitlesTrackId, const wchar_t* textCueId, const wchar_t* language, const wchar_t** textLines, unsigned int lineCount);

extern "C" typedef void(UNITY_INTERFACE_API *SubtitleItemExitedCallback)(
	_In_ void* pClientObject, const wchar_t* subtitlesTrackId, const wchar_t* textCueId);


typedef ABI::Windows::Foundation::ITypedEventHandler<ABI::Windows::Media::Playback::MediaPlayer*, IInspectable*> IMediaPlayerEventHandler;
typedef ABI::Windows::Foundation::ITypedEventHandler<ABI::Windows::Media::Playback::MediaPlayer*, ABI::Windows::Media::Playback::MediaPlayerFailedEventArgs*> IFailedEventHandler;
typedef ABI::Windows::Foundation::ITypedEventHandler<ABI::Windows::Media::Playback::MediaPlaybackSession*, IInspectable*> IMediaPlaybackSessionEventHandler;
typedef ABI::Windows::Foundation::ITypedEventHandler<ABI::Windows::Media::Streaming::Adaptive::AdaptiveMediaSource*, ABI::Windows::Media::Streaming::Adaptive::AdaptiveMediaSourceDownloadRequestedEventArgs*> IDownloadRequestedEventHandler;
typedef ABI::Windows::Foundation::ITypedEventHandler<ABI::Windows::Media::Playback::MediaPlaybackItem*, ABI::Windows::Foundation::Collections::IVectorChangedEventArgs*> ITracksChangedEventHandler;
typedef ABI::Windows::Foundation::ITypedEventHandler<ABI::Windows::Media::Core::TimedMetadataTrack*, ABI::Windows::Media::Core::MediaCueEventArgs*> IMediaCueEventHandler;

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
	STDMETHOD(IsHWDecodingSupported)(_Out_ BOOL* pSupportsHWVideoDecoding) PURE;
	STDMETHOD(SetDRMLicense)(_In_ LPCWSTR pszlicenseServiceURL, _In_ LPCWSTR pszCustomChallendgeData) PURE;
	STDMETHOD(SetDRMLicenseCallback)(_In_ DRMLicenseRequestedCallback fnCallback) PURE;
	STDMETHOD(GetSubtitlesTrackCount)(_Out_ unsigned int* count) PURE;
	STDMETHOD(GetSubtitlesTrack)(_In_ unsigned int index, _Out_ const wchar_t** trackId, _Out_ const wchar_t** trackLabel, _Out_ const wchar_t** trackLanguage) PURE;
	STDMETHOD(SetSubtitlesCallbacks)(_In_ SubtitleItemEnteredCallback fnEnteredCallback, _In_ SubtitleItemExitedCallback fnExitedCallback) PURE;
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
        _In_ IUnityGraphicsD3D11* pUnityDevice);

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
	IFACEMETHOD(IsHWDecodingSupported)(_Out_ BOOL* pSupportsHWVideoDecoding);

	IFACEMETHOD(SetDRMLicense)(_In_ LPCWSTR pszlicenseServiceURL, _In_ LPCWSTR pszCustomChallendgeData);
	IFACEMETHOD(SetDRMLicenseCallback)(_In_ DRMLicenseRequestedCallback fnCallback);

	IFACEMETHOD(GetSubtitlesTrackCount)(_Out_ unsigned int* count);
	IFACEMETHOD(GetSubtitlesTrack)(_In_ unsigned int index, _Out_ const wchar_t** trackId, _Out_ const wchar_t** trackLabel, _Out_ const wchar_t** trackLanguage);
	IFACEMETHOD(SetSubtitlesCallbacks)(_In_ SubtitleItemEnteredCallback fnEnteredCallback, _In_ SubtitleItemExitedCallback fnExitedCallback);

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

	HRESULT OnVideoTracksChanged(ABI::Windows::Media::Playback::IMediaPlaybackItem* pItem, ABI::Windows::Foundation::Collections::IVectorChangedEventArgs* pArgs);
	HRESULT OnTimedMetadataTracksChanged(ABI::Windows::Media::Playback::IMediaPlaybackItem* pItem, ABI::Windows::Foundation::Collections::IVectorChangedEventArgs* pArgs);
	HRESULT OnCueEntered(ABI::Windows::Media::Core::ITimedMetadataTrack* pTrack, ABI::Windows::Media::Core::IMediaCueEventArgs* pArgs);
	HRESULT OnCueExited(ABI::Windows::Media::Core::ITimedMetadataTrack* pTrack, ABI::Windows::Media::Core::IMediaCueEventArgs* pArgs);

	static void LicenseRequestInternal(void* objectThisPtr, Microsoft::WRL::Wrappers::HString& licenseUriResult, Microsoft::WRL::Wrappers::HString& licenseCustomChallendgeDataResult);

private:
    HRESULT CreateMediaPlayer();
    void ReleaseMediaPlayer();

	HRESULT InitializeDevices();

	HRESULT InitializeMediaPlayerWithPlayReadyDRM();

    HRESULT CreateTextures();
    void ReleaseTextures();

    HRESULT AddStateChanged();
    void RemoveStateChanged();

    void ReleaseResources();

private:
	IUnityGraphicsD3D11*				 m_pUnityGraphics;

    Microsoft::WRL::ComPtr<ID3D11Device> m_d3dDevice;
    Microsoft::WRL::ComPtr<ID3D11Device> m_mediaDevice;

    StateChangedCallback m_fnStateCallback;
	DRMLicenseRequestedCallback m_fnLicenseCallback;
	SubtitleItemEnteredCallback m_fnSubtitleEntered;
	SubtitleItemExitedCallback m_fnSubtitleExited;
	void* m_pClientObject;

    Microsoft::WRL::ComPtr<ABI::Windows::Media::Playback::IMediaPlayer> m_mediaPlayer;
    EventRegistrationToken m_openedEventToken;
    EventRegistrationToken m_endedEventToken;
    EventRegistrationToken m_failedEventToken;
    EventRegistrationToken m_videoFrameAvailableToken;
	EventRegistrationToken m_downloadRequestedEventToken;
	EventRegistrationToken m_videoTracksChangedEventToken;
	EventRegistrationToken m_timedMetadataChangedEventToken;

	bool m_bIgnoreEvents;
	PlayReadyHandler m_playreadyHandler;
	Microsoft::WRL::Wrappers::HString m_currentLicenseServiceURL;
	Microsoft::WRL::Wrappers::HString m_currentLicenseCustomChallendge;

#ifndef NO_FFMPEG
    Microsoft::WRL::ComPtr<FFmpegInterop::IFFmpegInteropMSS> m_ffmpegInteropMSS;
#endif

	Microsoft::WRL::ComPtr<ABI::Windows::Media::Streaming::Adaptive::IAdaptiveMediaSource> m_spAdaptiveMediaSource;
	Microsoft::WRL::ComPtr<ABI::Windows::Media::Playback::IMediaPlaybackItem> m_spPlaybackItem;

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

	std::vector<SUBTITLE_TRACK> m_subtitleTracks;

	bool m_gotDeviceLost;
	bool m_readyForFrames;
	bool m_noHWDecoding;
	bool m_make1080MaxWhenNoHWDecoding;
};

