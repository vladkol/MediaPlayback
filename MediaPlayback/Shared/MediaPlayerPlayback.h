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
#include <mutex>


enum class StateType : UINT32
{
    StateType_None = 0,
    StateType_Opened,
    StateType_StateChanged,
    StateType_Failed,
	StateType_NewFrameTexture,
	StateType_GraphicsDeviceShutdown,
	StateType_GraphicsDeviceReady
};

enum class PlaybackState : UINT32
{
    PlaybackState_None = 0,
    PlaybackState_Opening,
    PlaybackState_Buffering,
    PlaybackState_Playing,
    PlaybackState_Paused,
    PlaybackState_Ended, 
	PlaybackState_NA = 255
};

#pragma pack(push, 8)
typedef struct _MEDIA_DESCRIPTION
{
    UINT32 width;
    UINT32 height;
    INT64 duration;
    byte canSeek;
	byte isStereoscopic;
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


extern "C" typedef void(UNITY_INTERFACE_API *SubtitleItemEnteredCallback)(
	_In_ void* pClientObject, _In_ const wchar_t* subtitlesTrackId, _In_ const wchar_t* textCueId, _In_ const wchar_t* language, _In_ const wchar_t** textLines, _In_ unsigned int lineCount);

extern "C" typedef void(UNITY_INTERFACE_API *SubtitleItemExitedCallback)(
	_In_ void* pClientObject, _In_ const wchar_t* subtitlesTrackId, _In_ const wchar_t* textCueId);


typedef ABI::Windows::Foundation::ITypedEventHandler<ABI::Windows::Media::Playback::MediaPlayer*, IInspectable*> IMediaPlayerEventHandler;
typedef ABI::Windows::Foundation::ITypedEventHandler<ABI::Windows::Media::Playback::MediaPlayer*, ABI::Windows::Media::Playback::MediaPlayerFailedEventArgs*> IFailedEventHandler;
typedef ABI::Windows::Foundation::ITypedEventHandler<ABI::Windows::Media::Playback::MediaPlaybackSession*, IInspectable*> IMediaPlaybackSessionEventHandler;
typedef ABI::Windows::Foundation::ITypedEventHandler<ABI::Windows::Media::Streaming::Adaptive::AdaptiveMediaSource*, ABI::Windows::Media::Streaming::Adaptive::AdaptiveMediaSourceDownloadRequestedEventArgs*> IDownloadRequestedEventHandler;
typedef ABI::Windows::Foundation::ITypedEventHandler<ABI::Windows::Media::Playback::MediaPlaybackItem*, ABI::Windows::Foundation::Collections::IVectorChangedEventArgs*> ITracksChangedEventHandler;
typedef ABI::Windows::Foundation::ITypedEventHandler<ABI::Windows::Media::Core::TimedMetadataTrack*, ABI::Windows::Media::Core::MediaCueEventArgs*> IMediaCueEventHandler;

DECLARE_INTERFACE_IID_(IMediaPlayerPlayback, IUnknown, "9669c78e-42c4-4178-a1e3-75b03d0f8c9a")
{
    STDMETHOD(LoadContent)(_In_ LPCWSTR pszContentLocation) PURE;
    STDMETHOD(Play)() PURE;
    STDMETHOD(Pause)() PURE;
    STDMETHOD(Stop)() PURE;
	STDMETHOD(GetPlaybackTexture)(_Out_ IUnknown** d3d11TexturePtr, _Out_ LPBYTE isStereoscopic) PURE;
	STDMETHOD(GetDurationAndPosition)(_Out_ LONGLONG* duration, _Out_ LONGLONG* position) PURE;
	STDMETHOD(Seek)(_In_ LONGLONG position) PURE;
	STDMETHOD(SetVolume)(_In_ DOUBLE volume) PURE;
	STDMETHOD(GetIUnknown)(_Out_ IUnknown** ppUnknown) PURE;
	STDMETHOD(IsHardware4KDecodingSupported)(_Out_ BOOL* pSupportsHardware4KVideoDecoding) PURE;
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

	static void GraphicsDeviceShutdown();
	static void GraphicsDeviceReady(IUnityInterfaces* pUnityInterfaces);

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
    IFACEMETHOD(LoadContent)(_In_ LPCWSTR pszContentLocation);

    IFACEMETHOD(Play)();
    IFACEMETHOD(Pause)();
    IFACEMETHOD(Stop)();

	IFACEMETHOD(GetPlaybackTexture)(_Out_ IUnknown** d3d11TexturePtr, _Out_ LPBYTE isStereoscopic);

	IFACEMETHOD(GetDurationAndPosition)(_Out_ LONGLONG* duration, _Out_ LONGLONG* position);
	IFACEMETHOD(Seek)(_In_ LONGLONG position);
	IFACEMETHOD(SetVolume)(_In_ DOUBLE volume);

	IFACEMETHOD(GetIUnknown)(_Out_ IUnknown** ppUnknown);
	IFACEMETHOD(IsHardware4KDecodingSupported)(_Out_ BOOL* pSupportsHardware4KVideoDecoding);

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
	HRESULT OnSizeChanged(
		_In_ ABI::Windows::Media::Playback::IMediaPlaybackSession* sender,
		_In_ IInspectable* args);

	HRESULT OnDownloadRequested(
		_In_ ABI::Windows::Media::Streaming::Adaptive::IAdaptiveMediaSource* sender,
		_In_ ABI::Windows::Media::Streaming::Adaptive::IAdaptiveMediaSourceDownloadRequestedEventArgs* args);

	HRESULT OnVideoTracksChanged(ABI::Windows::Media::Playback::IMediaPlaybackItem* pItem, ABI::Windows::Foundation::Collections::IVectorChangedEventArgs* pArgs);
	HRESULT OnTimedMetadataTracksChanged(ABI::Windows::Media::Playback::IMediaPlaybackItem* pItem, ABI::Windows::Foundation::Collections::IVectorChangedEventArgs* pArgs);
	HRESULT OnCueEntered(ABI::Windows::Media::Core::ITimedMetadataTrack* pTrack, ABI::Windows::Media::Core::IMediaCueEventArgs* pArgs);
	HRESULT OnCueExited(ABI::Windows::Media::Core::ITimedMetadataTrack* pTrack, ABI::Windows::Media::Core::IMediaCueEventArgs* pArgs);

private:
	HRESULT CreatePlaybackTextures();

    HRESULT CreateMediaPlayer();
    void ReleaseMediaPlayer();

	HRESULT InitializeDevices();

    void ReleaseTextures();

    HRESULT AddStateChanged();
    void RemoveStateChanged();

    void ReleaseResources();

	void DeviceShutdown();
	HRESULT DeviceReady(IUnityGraphicsD3D11* unityD3D);

private:
	IUnityGraphicsD3D11*				 m_pUnityGraphics;

    Microsoft::WRL::ComPtr<ID3D11Device> m_d3dDevice;
    Microsoft::WRL::ComPtr<ID3D11Device> m_mediaDevice;
	Microsoft::WRL::ComPtr<IMFDXGIDeviceManager> m_spDeviceManager;

    StateChangedCallback m_fnStateCallback;
	SubtitleItemEnteredCallback m_fnSubtitleEntered;
	SubtitleItemExitedCallback m_fnSubtitleExited;
	void* m_pClientObject;

    Microsoft::WRL::ComPtr<ABI::Windows::Media::Playback::IMediaPlayer> m_mediaPlayer;
	Microsoft::WRL::ComPtr<ABI::Windows::Media::Playback::IMediaPlayer3> m_mediaPlayer3;
	Microsoft::WRL::ComPtr<ABI::Windows::Media::Playback::IMediaPlayer5> m_mediaPlayer5;

	Microsoft::WRL::ComPtr<ABI::Windows::Media::Playback::IMediaPlaybackSession> m_mediaPlaybackSession;

	UINT m_uiDeviceResetToken;
    EventRegistrationToken m_openedEventToken;
    EventRegistrationToken m_endedEventToken;
    EventRegistrationToken m_failedEventToken;
    EventRegistrationToken m_videoFrameAvailableToken;
	EventRegistrationToken m_downloadRequestedEventToken;
	EventRegistrationToken m_videoTracksChangedEventToken;
	EventRegistrationToken m_timedMetadataChangedEventToken;

	bool m_bIgnoreEvents;
	bool m_firstInitializationDone;

	Microsoft::WRL::ComPtr<ABI::Windows::Media::Streaming::Adaptive::IAdaptiveMediaSource> m_spAdaptiveMediaSource;
	Microsoft::WRL::ComPtr<ABI::Windows::Media::Playback::IMediaPlaybackItem> m_spPlaybackItem;
    
    EventRegistrationToken m_stateChangedEventToken;
	EventRegistrationToken m_sizeChangedEventToken;
	EventRegistrationToken m_durationChangedEventToken;

    CD3D11_TEXTURE2D_DESC m_textureDesc;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_primaryTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_primaryTextureSRV;


    HANDLE m_primarySharedHandle;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_primaryMediaTexture;
    Microsoft::WRL::ComPtr<ABI::Windows::Graphics::DirectX::Direct3D11::IDirect3DSurface> m_primaryMediaSurface;

	Microsoft::WRL::ComPtr<ID3D11Texture2D> m_leftEyeMediaTexture;
	Microsoft::WRL::ComPtr<ABI::Windows::Graphics::DirectX::Direct3D11::IDirect3DSurface> m_leftEyeMediaSurface;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> m_rightEyeMediaTexture;
	Microsoft::WRL::ComPtr<ABI::Windows::Graphics::DirectX::Direct3D11::IDirect3DSurface> m_rightEyeMediaSurface;


	std::vector<SUBTITLE_TRACK> m_subtitleTracks;

	bool m_readyForFrames;
	bool m_noHW4KDecoding;
	bool m_make1080MaxWhenNoHWDecoding;
	bool m_releasing;

private:
	static bool m_deviceNotReady;
	static std::vector<CMediaPlayerPlayback*> m_playbackObjects;
	static Microsoft::WRL::Wrappers::Mutex m_playbackVectorMutex;
};

