//*****************************************************************************
//
//	Copyright 2015 Microsoft Corporation
//
//	Licensed under the Apache License, Version 2.0 (the "License");
//	you may not use this file except in compliance with the License.
//	You may obtain a copy of the License at
//
//	http ://www.apache.org/licenses/LICENSE-2.0
//
//	Unless required by applicable law or agreed to in writing, software
//	distributed under the License is distributed on an "AS IS" BASIS,
//	WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//	See the License for the specific language governing permissions and
//	limitations under the License.
//
//*****************************************************************************

#pragma once
#include <queue>
#include <mutex>
#include "MediaSampleProvider.h"
#include "FFMpegInterop/FFmpegReader.h"

extern "C"
{
#include <libavformat/avformat.h>
}

namespace FFmpegInterop
{
    typedef ABI::Windows::Foundation::ITypedEventHandler<ABI::Windows::Media::Core::MediaStreamSource*, ABI::Windows::Media::Core::MediaStreamSourceStartingEventArgs*> IStartingEventHandler;
    typedef ABI::Windows::Foundation::ITypedEventHandler<ABI::Windows::Media::Core::MediaStreamSource*, ABI::Windows::Media::Core::MediaStreamSourceSampleRequestedEventArgs*> ISampleRequestedEventHandler;
    typedef ABI::Windows::Foundation::ITypedEventHandler<ABI::Windows::Media::Core::MediaStreamSource*, ABI::Windows::Media::Core::MediaStreamSourceClosedEventArgs*> IClosedEventHandler;

    DECLARE_INTERFACE_IID_(IFFmpegInteropMSS, IUnknown, "dc70dfca-89ab-46b0-aae0-77b0c16b6944")
    {
        STDMETHOD(get_MediaStreamSource)(_COM_Outptr_ ABI::Windows::Media::Core::IMediaStreamSource** ppMediaStreamSource) PURE;
    };

    class FFmpegInteropMSS
        : public Microsoft::WRL::RuntimeClass
        < Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>
        , IFFmpegInteropMSS
        , Microsoft::WRL::FtmBase>
    {
    public:
        // Contructor
        FFmpegInteropMSS();
        virtual ~FFmpegInteropMSS();

        HRESULT RuntimeClassInitialize(
            _In_ ABI::Windows::Storage::Streams::IRandomAccessStream* stream, 
            _In_ bool forceAudioDecode, 
            _In_ bool forceVideoDecode, 
            _In_opt_ ABI::Windows::Foundation::Collections::IPropertySet* ffmpegOptions);
        
        HRESULT RuntimeClassInitialize(
            _In_ LPCWSTR uri, 
            _In_ bool forceAudioDecode, 
            _In_ bool forceVideoDecode, 
            _In_opt_ ABI::Windows::Foundation::Collections::IPropertySet* ffmpegOptions);

        IFACEMETHOD(get_MediaStreamSource)(_COM_Outptr_ ABI::Windows::Media::Core::IMediaStreamSource** ppMediaStreamSource);

        // Properties
        ABI::Windows::Media::Core::IAudioStreamDescriptor* get_AudioDescriptor()
        {
            return spAudioStreamDescriptor.Get();
        };
        ABI::Windows::Media::Core::IVideoStreamDescriptor* get_VideoDescriptor()
        {
            return spVideoStreamDescriptor.Get();
        };
        ABI::Windows::Foundation::TimeSpan get_Duration()
        {
            return mediaDuration;
        };
        std::wstring get_VideoCodecName()
        {
            return videoCodecName;
        };
        std::wstring get_AudioCodecName()
        {
            return audioCodecName;
        };

    private:
        HRESULT InitFFmpegContext(_In_ bool forceAudioDecode, _In_ bool forceVideoDecode);
        HRESULT CreateAudioStreamDescriptor(_In_ bool forceAudioDecode);
        HRESULT CreateVideoStreamDescriptor(_In_ bool forceVideoDecode);
        HRESULT ConvertCodecName(_In_ const char* codecName, _In_ std::wstring& outputCodecName);
        HRESULT ParseOptions(_In_ ABI::Windows::Foundation::Collections::IPropertySet* ffmpegOptions);
        HRESULT OnStarting(_In_ ABI::Windows::Media::Core::IMediaStreamSource* sender, _In_ ABI::Windows::Media::Core::IMediaStreamSourceStartingEventArgs* args);
        HRESULT OnSampleRequested(_In_ ABI::Windows::Media::Core::IMediaStreamSource* sender, _In_ ABI::Windows::Media::Core::IMediaStreamSourceSampleRequestedEventArgs* args);
        HRESULT OnClosed(_In_ ABI::Windows::Media::Core::IMediaStreamSource* sender, _In_ ABI::Windows::Media::Core::IMediaStreamSourceClosedEventArgs* args);

    private:
        Microsoft::WRL::Wrappers::SRWLock m_lock;

        Microsoft::WRL::ComPtr<ABI::Windows::Media::Core::IMediaStreamSource> m_mediaStreamSource;
        EventRegistrationToken m_startingRequestedToken;
        EventRegistrationToken m_sampleRequestedToken;
        EventRegistrationToken m_closeRequestedToken;

        AVDictionary* avDict;
        AVIOContext* avIOCtx;
        AVFormatContext* avFormatCtx;
        AVCodecContext* avAudioCodecCtx;
        AVCodecContext* avVideoCodecCtx;

        int audioStreamIndex;
        Microsoft::WRL::ComPtr<ABI::Windows::Media::Core::IAudioStreamDescriptor> spAudioStreamDescriptor;

        int videoStreamIndex;
        Microsoft::WRL::ComPtr<ABI::Windows::Media::Core::IVideoStreamDescriptor> spVideoStreamDescriptor;

        bool rotateVideo;
        int rotationAngle;

        std::shared_ptr<MediaSampleProvider> m_audioSampleProvider;
        std::shared_ptr<MediaSampleProvider> m_videoSampleProvider;

        std::wstring videoCodecName;
        std::wstring audioCodecName;
        ABI::Windows::Foundation::TimeSpan mediaDuration;
        IStream* fileStreamData;
        unsigned char* fileStreamBuffer;
        std::shared_ptr<FFmpegReader> m_spReader;
    };
}
