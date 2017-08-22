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

#include "pch.h"

#ifndef NO_FFMPEG

#include "FFmpegInteropMSS.h"
#include "MediaSampleProvider.h"
#include "H264AVCSampleProvider.h"
#include "H264SampleProvider.h"
#include "UncompressedAudioSampleProvider.h"
#include "UncompressedVideoSampleProvider.h"
#include "shcore.h"

extern "C"
{
#include <libavutil/imgutils.h>
}

using namespace FFmpegInterop;
using namespace ABI::Windows::Foundation::Collections;
using namespace ABI::Windows::Media;
using namespace ABI::Windows::Media::Core;
using namespace ABI::Windows::Media::MediaProperties;
using namespace ABI::Windows::Storage::Streams;
using namespace Microsoft::WRL;

// Size of the buffer when reading a stream
const int FILESTREAMBUFFERSZ = 16384;

// Static functions passed to FFmpeg for stream interop
static int FileStreamRead(_In_ void* ptr, _In_ uint8_t* buf, _In_ int bufSize);
static int64_t FileStreamSeek(_In_ void* ptr, _In_ int64_t pos, _In_ int whence);

// Initialize an FFmpegInteropObject
_Use_decl_annotations_
FFmpegInteropMSS::FFmpegInteropMSS()
    : avDict(nullptr)
    , avIOCtx(nullptr)
    , avFormatCtx(nullptr)
    , avAudioCodecCtx(nullptr)
    , avVideoCodecCtx(nullptr)
    , audioStreamIndex(AVERROR_STREAM_NOT_FOUND)
    , videoStreamIndex(AVERROR_STREAM_NOT_FOUND)
    , fileStreamData(nullptr)
    , fileStreamBuffer(nullptr)
{
    av_register_all();
}

_Use_decl_annotations_
FFmpegInteropMSS::~FFmpegInteropMSS()
{
    auto lock = m_lock.LockExclusive();

    if (m_mediaStreamSource != nullptr)
    {
        m_mediaStreamSource->remove_Starting(m_startingRequestedToken);
        m_mediaStreamSource->remove_SampleRequested(m_sampleRequestedToken);
        m_mediaStreamSource->remove_Closed(m_closeRequestedToken);
        m_mediaStreamSource.Reset();
        m_mediaStreamSource = nullptr;
    }

    // Clear our data
    if (nullptr != m_audioSampleProvider)
    {
        m_audioSampleProvider.reset();
        m_audioSampleProvider = nullptr;
    }

    if (nullptr != m_videoSampleProvider)
    {
        m_videoSampleProvider.reset();
        m_videoSampleProvider = nullptr;
    }

    auto locked = m_spReader->GetWeakPtr<FFmpegReader>().lock();
    if (locked != nullptr)
    {
        //locked->SetAudioStream(AVERROR_STREAM_NOT_FOUND, m_audioSampleProvider);
        //locked->SetVideoStream(AVERROR_STREAM_NOT_FOUND, m_videoSampleProvider);
        locked.reset();
        locked = nullptr;
    }

    avcodec_close(avVideoCodecCtx);
    avcodec_close(avAudioCodecCtx);
    avformat_close_input(&avFormatCtx);
    av_free(avIOCtx);
    av_dict_free(&avDict);

    avformat_network_deinit();

    if (fileStreamData != nullptr)
    {
        fileStreamData->Release();
    }
}

_Use_decl_annotations_
HRESULT FFmpegInteropMSS::RuntimeClassInitialize(
    LPCWSTR uri,
    bool forceAudioDecode,
    bool forceVideoDecode,
    ABI::Windows::Foundation::Collections::IPropertySet* ffmpegOptions)
{
    if (nullptr == uri)
    {
        IFR(E_INVALIDARG);
    }

    auto lock = m_lock.LockExclusive();
    avformat_network_init();

    avFormatCtx = avformat_alloc_context();
    if (avFormatCtx == nullptr)
    {
        IFR(E_OUTOFMEMORY);
    }

    // Populate AVDictionary avDict based on PropertySet ffmpegOptions. List of options can be found in https://www.ffmpeg.org/ffmpeg-protocols.html
    IFR(ParseOptions(ffmpegOptions));

    std::wstring uriW(uri);
    std::string uriA(uriW.begin(), uriW.end());
    const char* charStr = uriA.c_str();

    // Open media in the given URI using the specified options
    if (avformat_open_input(&avFormatCtx, charStr, NULL, &avDict) < 0)
    {
		if (avFormatCtx != nullptr)
		{
			avformat_free_context(avFormatCtx);
			avFormatCtx = nullptr;
		}
        IFR(E_FAIL); // Error opening file
    }

    // avDict is not NULL only when there is an issue with the given ffmpegOptions such as invalid key, value type etc. Iterate through it to see which one is causing the issue.
    if (avDict != nullptr)
    {
        DebugMessage(L"Invalid FFmpeg option(s)");
        av_dict_free(&avDict);
        avDict = nullptr;
    }

    IFR(InitFFmpegContext(forceAudioDecode, forceVideoDecode));

    return S_OK;
}

_Use_decl_annotations_
HRESULT FFmpegInteropMSS::RuntimeClassInitialize(
    ABI::Windows::Storage::Streams::IRandomAccessStream* stream,
    bool forceAudioDecode,
    bool forceVideoDecode,
    ABI::Windows::Foundation::Collections::IPropertySet* ffmpegOptions)
{
    if (nullptr != stream)
    {
        IFR(E_INVALIDARG);
    }

    auto lock = m_lock.LockExclusive();

    // Convert asynchronous IRandomAccessStream to synchronous IStream. This API requires shcore.h and shcore.lib
    IFR(CreateStreamOverRandomAccessStream(reinterpret_cast<IUnknown*>(stream), IID_PPV_ARGS(&fileStreamData)));

    // Setup FFmpeg custom IO to access file as stream. This is necessary when accessing any file outside of app installation directory and appdata folder.
    // Credit to Philipp Sch http://www.codeproject.com/Tips/489450/Creating-Custom-FFmpeg-IO-Context
    fileStreamBuffer = (unsigned char*)av_malloc(FILESTREAMBUFFERSZ);
    if (fileStreamBuffer == nullptr)
    {
        IFR(E_OUTOFMEMORY);
    }

    avIOCtx = avio_alloc_context(fileStreamBuffer, FILESTREAMBUFFERSZ, 0, fileStreamData, FileStreamRead, 0, FileStreamSeek);
    if (avIOCtx == nullptr)
    {
        IFR(E_OUTOFMEMORY);
    }

    avFormatCtx = avformat_alloc_context();
    if (avFormatCtx == nullptr)
    {
        IFR(E_OUTOFMEMORY);
    }

    // Populate AVDictionary avDict based on PropertySet ffmpegOptions. List of options can be found in https://www.ffmpeg.org/ffmpeg-protocols.html
    IFR(ParseOptions(ffmpegOptions));

    avFormatCtx->pb = avIOCtx;
    avFormatCtx->flags |= AVFMT_FLAG_CUSTOM_IO;

    // Open media file using custom IO setup above instead of using file name. Opening a file using file name will invoke fopen C API call that only have
    // access within the app installation directory and appdata folder. Custom IO allows access to file selected using FilePicker dialog.
    if (avformat_open_input(&avFormatCtx, "", NULL, &avDict) < 0)
    {
		if (avFormatCtx != nullptr)
		{
			avformat_free_context(avFormatCtx);
			avFormatCtx = nullptr;
		}
		IFR(E_FAIL); // Error opening file
    }

    // avDict is not NULL only when there is an issue with the given ffmpegOptions such as invalid key, value type etc. Iterate through it to see which one is causing the issue.
    if (avDict != nullptr)
    {
        Log(Log_Level_Error, L"Invalid FFmpeg option(s)");
        av_dict_free(&avDict);
        avDict = nullptr;
    }

    IFR(InitFFmpegContext(forceAudioDecode, forceVideoDecode));

    return S_OK;
}

_Use_decl_annotations_
HRESULT FFmpegInteropMSS::get_MediaStreamSource(
    ABI::Windows::Media::Core::IMediaStreamSource** ppMediaStreamSource)
{
    auto lock = m_lock.LockShared();

    return m_mediaStreamSource.CopyTo(ppMediaStreamSource);
}

_Use_decl_annotations_
HRESULT FFmpegInteropMSS::InitFFmpegContext(bool forceAudioDecode, bool forceVideoDecode)
{
    if (avformat_find_stream_info(avFormatCtx, NULL) < 0)
    {
        IFR(E_FAIL); // Error finding info
    }

    m_spReader = std::make_unique<FFmpegReader>(avFormatCtx);
    if (m_spReader == nullptr)
    {
        IFR(E_OUTOFMEMORY);
    }

    // Find the audio stream and its decoder
    AVCodec* avAudioCodec = nullptr;
    audioStreamIndex = av_find_best_stream(avFormatCtx, AVMEDIA_TYPE_AUDIO, -1, -1, &avAudioCodec, 0);
    if (audioStreamIndex != AVERROR_STREAM_NOT_FOUND && avAudioCodec)
    {
        HRESULT hr = S_OK;

        // allocate a new decoding context
        avAudioCodecCtx = avcodec_alloc_context3(avAudioCodec);
        if (!avAudioCodecCtx)
        {
            hr = E_OUTOFMEMORY;
            DebugMessage(L"Could not allocate a decoding context\n");
            avformat_close_input(&avFormatCtx);
        }

        if (SUCCEEDED(hr))
        {
            // initialize the stream parameters with demuxer information
            if (avcodec_parameters_to_context(avAudioCodecCtx, avFormatCtx->streams[audioStreamIndex]->codecpar) < 0)
            {
                hr = E_FAIL;
                avformat_close_input(&avFormatCtx);
                avcodec_free_context(&avAudioCodecCtx);
            }

            if (SUCCEEDED(hr))
            {
                if (avcodec_open2(avAudioCodecCtx, avAudioCodec, NULL) < 0)
                {
                    avAudioCodecCtx = nullptr;
                    hr = E_FAIL;
                }
                else
                {
                    // Detect audio format and create audio stream descriptor accordingly
                    hr = CreateAudioStreamDescriptor(forceAudioDecode);
                    if (SUCCEEDED(hr))
                    {
                        auto weakPtr = m_audioSampleProvider->GetWeakPtr<MediaSampleProvider>();
                        auto locked = weakPtr.lock();
                        if (locked != nullptr)
                        {
                            hr = locked->AllocateResources();
                            if (SUCCEEDED(hr))
                            {
                                m_spReader->SetAudioStream(audioStreamIndex, weakPtr);
                            }
                        }
                    }

                    if (SUCCEEDED(hr))
                    {
                        // Convert audio codec name for property
                        hr = ConvertCodecName(avAudioCodec->name, audioCodecName);
                    }
                }
            }
        }

        if (FAILED(hr))
        {
            IFR(hr);
        }
    }

    // Find the video stream and its decoder
    AVCodec* avVideoCodec = nullptr;
    videoStreamIndex = av_find_best_stream(avFormatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, &avVideoCodec, 0);
    if (videoStreamIndex != AVERROR_STREAM_NOT_FOUND && avVideoCodec)
    {
        HRESULT hr = S_OK;

        // FFmpeg identifies album/cover art from a music file as a video stream
        // Avoid creating unnecessarily video stream from this album/cover art
        if (avFormatCtx->streams[videoStreamIndex]->disposition == AV_DISPOSITION_ATTACHED_PIC)
        {
            videoStreamIndex = AVERROR_STREAM_NOT_FOUND;
            avVideoCodec = nullptr;
        }
        else
        {
            AVDictionaryEntry *rotate_tag = av_dict_get(avFormatCtx->streams[videoStreamIndex]->metadata, "rotate", NULL, 0);
            if (rotate_tag != NULL)
            {
                rotateVideo = true;
                rotationAngle = atoi(rotate_tag->value);
            }
            else
            {
                rotateVideo = false;
            }

            // allocate a new decoding context
            avVideoCodecCtx = avcodec_alloc_context3(avVideoCodec);
            if (!avVideoCodecCtx)
            {
                DebugMessage(L"Could not allocate a decoding context\n");
                avformat_close_input(&avFormatCtx);
                hr = E_OUTOFMEMORY;
            }

            if (SUCCEEDED(hr))
            {
                // initialize the stream parameters with demuxer information
                if (avcodec_parameters_to_context(avVideoCodecCtx, avFormatCtx->streams[videoStreamIndex]->codecpar) < 0)
                {
                    avformat_close_input(&avFormatCtx);
                    avcodec_free_context(&avVideoCodecCtx);
                    hr = E_FAIL;
                }
            }

            if (SUCCEEDED(hr))
            {
                if (avcodec_open2(avVideoCodecCtx, avVideoCodec, NULL) < 0)
                {
                    avVideoCodecCtx = nullptr;
                    hr = E_FAIL; // Cannot open the video codec
                }
                else
                {
                    // Detect video format and create video stream descriptor accordingly
                    hr = CreateVideoStreamDescriptor(forceVideoDecode);
                    if (SUCCEEDED(hr))
                    {
                        auto weakPtr = m_videoSampleProvider->GetWeakPtr<MediaSampleProvider>();
                        auto locked = weakPtr.lock();
                        if (locked != nullptr)
                        {
                            hr = locked->AllocateResources();
                            if (SUCCEEDED(hr))
                            {
                                m_spReader->SetVideoStream(videoStreamIndex, weakPtr);
                            }
                        }
                    }

                    if (SUCCEEDED(hr))
                    {
                        // Convert video codec name for property
                        hr = ConvertCodecName(avVideoCodec->name, videoCodecName);
                    }
                }
            }
        }

        if (FAILED(hr))
        {
            IFR(hr);
        }
    }

    // Convert media duration from AV_TIME_BASE to TimeSpan unit
    mediaDuration = { LONGLONG(avFormatCtx->duration * 10000000 / double(AV_TIME_BASE)) };
        
    ComPtr<IMediaStreamSourceFactory> spMSSFactory;
    IFR(Windows::Foundation::GetActivationFactory(
        Wrappers::HStringReference(RuntimeClass_Windows_Media_Core_MediaStreamSource).Get(), 
        &spMSSFactory));

    if (spAudioStreamDescriptor != nullptr)
    {
        if (spVideoStreamDescriptor != nullptr)
        {
            ComPtr<IMediaStreamDescriptor> spVideoDescriptor;
            spVideoStreamDescriptor.As(&spVideoDescriptor);

            ComPtr<IMediaStreamDescriptor> spAudioDescriptor;
            spAudioStreamDescriptor.As(&spAudioDescriptor);

            IFR(spMSSFactory->CreateFromDescriptors(spVideoDescriptor.Get(), spAudioDescriptor.Get(), &m_mediaStreamSource));
        }
        else
        {
            ComPtr<IMediaStreamDescriptor> spAudioDescriptor;
            spAudioStreamDescriptor.As(&spAudioDescriptor);

            IFR(spMSSFactory->CreateFromDescriptor(spAudioDescriptor.Get(), &m_mediaStreamSource));
        }
    }
    else if (spVideoStreamDescriptor != nullptr)
    {
        ComPtr<IMediaStreamDescriptor> spVideoDescriptor;
        spVideoStreamDescriptor.As(&spVideoDescriptor);

        IFR(spMSSFactory->CreateFromDescriptor(spVideoDescriptor.Get(), &m_mediaStreamSource));
    }

    if (mediaDuration.Duration > 0)
    {
        IFR(m_mediaStreamSource->put_Duration(mediaDuration));
        IFR(m_mediaStreamSource->put_CanSeek(true));
    }
    else
    {
        // Set buffer time to 0 for realtime streaming to reduce latency
        m_mediaStreamSource->put_BufferTime({ 0 });
    }

    // setup callbacks
    auto startingCallback = 
        Microsoft::WRL::Callback<IStartingEventHandler>(this, &FFmpegInteropMSS::OnStarting);
    auto sampleResquestedCallback =
        Microsoft::WRL::Callback<ISampleRequestedEventHandler>(this, &FFmpegInteropMSS::OnSampleRequested);
    auto closedCallback = 
        Microsoft::WRL::Callback<IClosedEventHandler>(this, &FFmpegInteropMSS::OnClosed);

    IFR(m_mediaStreamSource->add_Starting(startingCallback.Get(), &m_startingRequestedToken));
    IFR(m_mediaStreamSource->add_SampleRequested(sampleResquestedCallback.Get(), &m_sampleRequestedToken));
    IFR(m_mediaStreamSource->add_Closed(closedCallback.Get(), &m_closeRequestedToken));

    return S_OK;
}

_Use_decl_annotations_
HRESULT FFmpegInteropMSS::ConvertCodecName(const char* codecName, std::wstring& outputCodecName)
{
    HRESULT hr = S_OK;

    // Convert codec name from const char* to Platform::String
    auto codecNameChars = codecName;
    size_t newsize = strlen(codecNameChars) + 1;
    wchar_t * wcstring = nullptr;

    try
    {
        wcstring = new wchar_t[newsize];
    }
    catch (std::bad_alloc&)
    {
        hr = E_FAIL; // couldn't allocate memory for codec name
    }

    if (SUCCEEDED(hr))
    {
        size_t convertedChars = 0;
        mbstowcs_s(&convertedChars, wcstring, newsize, codecNameChars, _TRUNCATE);
        outputCodecName = wcstring;
        delete[] wcstring;
    }

    return hr;
}

_Use_decl_annotations_
HRESULT FFmpegInteropMSS::CreateAudioStreamDescriptor(bool forceAudioDecode)
{
    ComPtr<IAudioEncodingPropertiesStatics> spFactory;
    HRESULT hr = Windows::Foundation::GetActivationFactory(Microsoft::WRL::Wrappers::HStringReference(RuntimeClass_Windows_Media_MediaProperties_AudioEncodingProperties).Get(), &spFactory);
    if (FAILED(hr))
        return hr;

    ComPtr<IAudioStreamDescriptorFactory> spAudioFactory;
    hr = Windows::Foundation::GetActivationFactory(Microsoft::WRL::Wrappers::HStringReference(RuntimeClass_Windows_Media_Core_AudioStreamDescriptor).Get(), &spAudioFactory);
    if (FAILED(hr))
        return hr;

    if (avAudioCodecCtx->codec_id == AV_CODEC_ID_AAC && !forceAudioDecode)
    {
        if (avAudioCodecCtx->extradata_size == 0)
        {
            ComPtr<IAudioEncodingProperties> spAudioProperties;
            hr = spFactory->CreateAacAdts(avAudioCodecCtx->sample_rate, avAudioCodecCtx->channels, static_cast<uint32_t>(avAudioCodecCtx->bit_rate), &spAudioProperties);
            if (FAILED(hr))
                return hr;

            hr = spAudioFactory->Create(spAudioProperties.Get(), &spAudioStreamDescriptor);
            if (FAILED(hr))
                return hr;
        }
        else
        {
            ComPtr<IAudioEncodingProperties> spAudioProperties;
            hr = spFactory->CreateAac(avAudioCodecCtx->sample_rate, avAudioCodecCtx->channels, static_cast<uint32_t>(avAudioCodecCtx->bit_rate), &spAudioProperties);
            if (FAILED(hr))
                return hr;

            hr = spAudioFactory->Create(spAudioProperties.Get(), &spAudioStreamDescriptor);
            if (FAILED(hr))
                return hr;
        }
        m_audioSampleProvider.reset(reinterpret_cast<MediaSampleProvider*>(new H264AVCSampleProvider(m_spReader->GetWeakPtr<FFmpegReader>(), avFormatCtx, avAudioCodecCtx)));
    }
    else if (avAudioCodecCtx->codec_id == AV_CODEC_ID_MP3 && !forceAudioDecode)
    {
        ComPtr<IAudioEncodingProperties> spAudioProperties;
        hr = spFactory->CreateMp3(avAudioCodecCtx->sample_rate, avAudioCodecCtx->channels, static_cast<uint32_t>(avAudioCodecCtx->bit_rate), &spAudioProperties);
        if (FAILED(hr))
            return hr;

        hr = spAudioFactory->Create(spAudioProperties.Get(), &spAudioStreamDescriptor);
        if (FAILED(hr))
            return hr;

        m_audioSampleProvider.reset(reinterpret_cast<MediaSampleProvider*>(new UncompressedAudioSampleProvider(m_spReader->GetWeakPtr<FFmpegReader>(), avFormatCtx, avAudioCodecCtx)));
    }
    else
    {
        // We always convert to 16-bit audio so set the size here
        ComPtr<IAudioEncodingProperties> spAudioProperties;
        hr = spFactory->CreatePcm(avAudioCodecCtx->sample_rate, avAudioCodecCtx->channels, 16, &spAudioProperties);
        if (FAILED(hr))
            return hr;

        hr = spAudioFactory->Create(spAudioProperties.Get(), &spAudioStreamDescriptor);
        if (FAILED(hr))
            return hr;

        m_audioSampleProvider.reset(reinterpret_cast<MediaSampleProvider*>(new UncompressedAudioSampleProvider(m_spReader->GetWeakPtr<FFmpegReader>(), avFormatCtx, avAudioCodecCtx)));
    }

    return (spAudioStreamDescriptor != nullptr && m_audioSampleProvider != nullptr) ? S_OK : E_OUTOFMEMORY;
}

_Use_decl_annotations_
HRESULT FFmpegInteropMSS::CreateVideoStreamDescriptor(bool forceVideoDecode)
{
    ComPtr<IVideoEncodingPropertiesStatics> spVideoEncodingPropertiesStatics;
    IFR(Windows::Foundation::GetActivationFactory(
        Wrappers::HStringReference(RuntimeClass_Windows_Media_MediaProperties_VideoEncodingProperties).Get(), 
        &spVideoEncodingPropertiesStatics));

    ComPtr<IVideoEncodingProperties> spVideoProps;
    if (avVideoCodecCtx->codec_id == AV_CODEC_ID_H264 && !forceVideoDecode)
    {
        IFR(spVideoEncodingPropertiesStatics->CreateH264(&spVideoProps));

        IFR(spVideoProps->put_Height(avVideoCodecCtx->height));

        IFR(spVideoProps->put_Width(avVideoCodecCtx->width));

        ComPtr<IVideoEncodingProperties2> spVideoProps2;
        IFR(spVideoProps.As(&spVideoProps2));

        IFR(spVideoProps2->put_ProfileId(avVideoCodecCtx->profile));

        // Check for H264 bitstream flavor. H.264 AVC extradata starts with 1 while non AVC one starts with 0
        if (avVideoCodecCtx->extradata != nullptr && avVideoCodecCtx->extradata_size > 0 && avVideoCodecCtx->extradata[0] == 1)
        {
            m_videoSampleProvider.reset(reinterpret_cast<MediaSampleProvider*>(new H264AVCSampleProvider(m_spReader->GetWeakPtr<FFmpegReader>(), avFormatCtx, avVideoCodecCtx)));
        }
        else
        {
            m_videoSampleProvider.reset(reinterpret_cast<MediaSampleProvider*>(new H264SampleProvider(m_spReader->GetWeakPtr<FFmpegReader>(), avFormatCtx, avVideoCodecCtx)));
        }
    }
    else
    {
        ComPtr<IMediaEncodingSubtypesStatics> spSubtypesStatics;
        IFR(Windows::Foundation::GetActivationFactory(
            Wrappers::HStringReference(RuntimeClass_Windows_Media_MediaProperties_MediaEncodingSubtypes).Get(), 
            &spSubtypesStatics));

        SafeString nv12;
        IFR(spSubtypesStatics->get_Nv12(nv12.GetAddressOf()));

        IFR(spVideoEncodingPropertiesStatics->CreateUncompressed(nv12, avVideoCodecCtx->width, avVideoCodecCtx->height, &spVideoProps));

        
        m_videoSampleProvider.reset(reinterpret_cast<MediaSampleProvider*>(new UncompressedVideoSampleProvider(m_spReader->GetWeakPtr<FFmpegReader>(), avFormatCtx, avVideoCodecCtx)));

        if (avVideoCodecCtx->sample_aspect_ratio.num > 0 && avVideoCodecCtx->sample_aspect_ratio.den != 0)
        {
            ComPtr<ABI::Windows::Media::MediaProperties::IMediaRatio> spAspectRatio;
            IFR(spVideoProps->get_PixelAspectRatio(&spAspectRatio));

            IFR(spAspectRatio->put_Numerator(avVideoCodecCtx->sample_aspect_ratio.num));

            IFR(spAspectRatio->put_Denominator(avVideoCodecCtx->sample_aspect_ratio.den));
        }
    }

    if (rotateVideo)
    {
        ComPtr<IMediaEncodingProperties> spMediaEncodingProps;
        IFR(spVideoProps.As(&spMediaEncodingProps));

        ComPtr<IMap<GUID, IInspectable*>> spEncodingPropertySet;
        IFR(spMediaEncodingProps->get_Properties(&spEncodingPropertySet));

        ComPtr<ABI::Windows::Foundation::IPropertyValueStatics> spPropertyValueFactory;
        IFR(ABI::Windows::Foundation::GetActivationFactory(
            Wrappers::HStringReference(RuntimeClass_Windows_Foundation_PropertyValue).Get(),
            &spPropertyValueFactory));

        ComPtr<ABI::Windows::Foundation::IPropertyValue> spPropertyValue;
        IFR(spPropertyValueFactory->CreateUInt32(rotationAngle, &spPropertyValue));

        boolean replaced = false;
        IFR(spEncodingPropertySet->Insert(MF_MT_VIDEO_ROTATION, spPropertyValue.Get(), &replaced));
    }

    ComPtr<ABI::Windows::Media::MediaProperties::IMediaRatio> spFrameRate;
    IFR(spVideoProps->get_FrameRate(&spFrameRate));

    // Detect the correct framerate
    if (avVideoCodecCtx->framerate.num != 0 && avVideoCodecCtx->framerate.den != 0)
    {
        IFR(spFrameRate->put_Numerator(avVideoCodecCtx->framerate.num));

        IFR(spFrameRate->put_Denominator(avVideoCodecCtx->framerate.den));
    }
    else if (avFormatCtx->streams[videoStreamIndex]->avg_frame_rate.num != 0 && avFormatCtx->streams[videoStreamIndex]->avg_frame_rate.den != 0)
    {
        IFR(spFrameRate->put_Numerator(avFormatCtx->streams[videoStreamIndex]->avg_frame_rate.num));

        IFR(spFrameRate->put_Denominator(avFormatCtx->streams[videoStreamIndex]->avg_frame_rate.den));
    }

    if (avVideoCodecCtx->bit_rate > 0)
    {
        IFR(spVideoProps->put_Bitrate((unsigned int)avVideoCodecCtx->bit_rate));
    }

    ComPtr<IVideoStreamDescriptorFactory> spVideoDescFactory;
    IFR(Windows::Foundation::GetActivationFactory(Microsoft::WRL::Wrappers::HStringReference(RuntimeClass_Windows_Media_Core_VideoStreamDescriptor).Get(), &spVideoDescFactory));

    IFR(spVideoDescFactory->Create(spVideoProps.Get(), &spVideoStreamDescriptor));

    return (spVideoStreamDescriptor != nullptr && m_videoSampleProvider != nullptr) ? S_OK : E_OUTOFMEMORY;
}

_Use_decl_annotations_
HRESULT FFmpegInteropMSS::ParseOptions(IPropertySet* ffmpegOptions)
{
    // Convert FFmpeg options given in PropertySet to AVDictionary. List of options can be found in https://www.ffmpeg.org/ffmpeg-protocols.html
    if (ffmpegOptions != nullptr)
    {
        ComPtr<IPropertySet> spOriginalProperties(ffmpegOptions);

        ComPtr<IIterator<IKeyValuePair<HSTRING, IInspectable*>*>> it;
        ComPtr<IIterable<IKeyValuePair<HSTRING, IInspectable*>*>> iterable;
        boolean hasCurrent;

        IFR(spOriginalProperties.As(&iterable));
        iterable->First(&it);

        for (HRESULT hr = it->get_HasCurrent(&hasCurrent); SUCCEEDED(hr) && hasCurrent; hr = it->MoveNext(&hasCurrent))
        {
            ComPtr<IKeyValuePair<HSTRING, IInspectable*>> originalElement;
            IFR(it->get_Current(&originalElement));

            Microsoft::WRL::Wrappers::HString key;
            IFR(originalElement->get_Key(key.GetAddressOf()));

            ComPtr<IInspectable> inspectableValue;;
            IFR(originalElement->get_Value(&inspectableValue));

            unsigned int length = 0;
            std::wstring keyW(key.GetRawBuffer(&length));
            std::string keyA(keyW.begin(), keyW.end());
            const char* keyChar = keyA.c_str();

            // Convert value from Object^ to const char*. avformat_open_input will internally convert value from const char* to the correct type
            Microsoft::WRL::Wrappers::HString value;
            IFR(inspectableValue->GetRuntimeClassName(value.GetAddressOf()));

            std::wstring valueW(value.GetRawBuffer(&length));
            std::string valueA(valueW.begin(), valueW.end());
            const char* valueChar = valueA.c_str();

            // Add key and value pair entry
            if (av_dict_set(&avDict, keyChar, valueChar, 0) < 0)
            {
                IFR(E_INVALIDARG);
            }
        }
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT FFmpegInteropMSS::OnStarting(IMediaStreamSource* sender, IMediaStreamSourceStartingEventArgs* args)
{
    ComPtr<IMediaStreamSourceStartingRequest> spRequest;
    IFR(args->get_Request(&spRequest));

    ComPtr<ABI::Windows::Foundation::IReference<ABI::Windows::Foundation::TimeSpan>> spStartPosition;
    IFR(spRequest->get_StartPosition(&spStartPosition));

    ABI::Windows::Foundation::TimeSpan value;
    if (spStartPosition != nullptr)
    {
        IFR(spStartPosition->get_Value(&value));
    }

    auto lock = m_lock.LockShared();

    // Perform seek operation when MediaStreamSource received seek event from MediaElement
    if (spStartPosition != nullptr && value.Duration <= mediaDuration.Duration)
    {
        // Select the first valid stream either from video or audio
        int streamIndex = videoStreamIndex >= 0 ? videoStreamIndex : audioStreamIndex >= 0 ? audioStreamIndex : -1;

        if (streamIndex >= 0)
        {
            // Convert TimeSpan unit to AV_TIME_BASE
            int64_t seekTarget = static_cast<int64_t>(value.Duration / (av_q2d(avFormatCtx->streams[streamIndex]->time_base) * 10000000));

            if (av_seek_frame(avFormatCtx, streamIndex, seekTarget, 0) < 0)
            {
                Log(Log_Level_Error, L" - ### Error while seeking\n");
            }
            else
            {
                // Add deferral

                // Flush the AudioSampleProvider
                if (m_audioSampleProvider != nullptr)
                {
                    m_audioSampleProvider->Flush();
                    avcodec_flush_buffers(avAudioCodecCtx);
                }

                // Flush the VideoSampleProvider
                if (m_videoSampleProvider != nullptr)
                {
                    m_videoSampleProvider->Flush();
                    avcodec_flush_buffers(avVideoCodecCtx);
                }
            }
        }

        IFR(spRequest->SetActualStartPosition(value));
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT FFmpegInteropMSS::OnSampleRequested(IMediaStreamSource* sender, IMediaStreamSourceSampleRequestedEventArgs* args)
{

    ComPtr<IMediaStreamSourceSampleRequest> spRequest;
    IFR(args->get_Request(&spRequest));

    ComPtr<IMediaStreamDescriptor> spDescriptor;
    IFR(spRequest->get_StreamDescriptor(&spDescriptor));

    ComPtr<IAudioStreamDescriptor> spAudioDescriptor;
    bool isAudioRequest = SUCCEEDED(spDescriptor.As(&spAudioDescriptor));

    ComPtr<IVideoStreamDescriptor> spVideoDescriptor;
    bool isVideoRequest = SUCCEEDED(spDescriptor.As(&spVideoDescriptor));

    auto lock = m_lock.LockShared();
    if (m_mediaStreamSource != nullptr)
    {
        ComPtr<IMediaStreamSample> spSample = nullptr;
        if (isAudioRequest && m_audioSampleProvider != nullptr)
        {
            IFR(m_audioSampleProvider->GetNextSample(&spSample));
        }
        else if (isVideoRequest && m_videoSampleProvider != nullptr)
        {
            IFR(m_videoSampleProvider->GetNextSample(&spSample));
        }
        IFR(spRequest->put_Sample(spSample.Get()));
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT FFmpegInteropMSS::OnClosed(IMediaStreamSource* sender, IMediaStreamSourceClosedEventArgs* args)
{
    auto lock = m_lock.LockShared();

    ComPtr<IMediaStreamSourceClosedRequest> spRequest;
    IFR(args->get_Request(&spRequest));

    MediaStreamSourceClosedReason reason;
    IFR(spRequest->get_Reason(&reason));

    switch (reason)
    {
    case MediaStreamSourceClosedReason_UnknownError:
        LOG_RESULT(E_UNEXPECTED)
        break;
    case MediaStreamSourceClosedReason_AppReportedError:
        LOG_RESULT(E_ABORT);
        break;
    case MediaStreamSourceClosedReason_UnsupportedProtectionSystem:
    case MediaStreamSourceClosedReason_ProtectionSystemFailure:
        LOG_RESULT(MF_E_DRM_UNSUPPORTED);
        break;
    case MediaStreamSourceClosedReason_UnsupportedEncodingFormat:
        LOG_RESULT(MF_E_UNSUPPORTED_FORMAT);
        break;
    case MediaStreamSourceClosedReason_MissingSampleRequestedEventHandler:
        LOG_RESULT(HRESULT_FROM_WIN32(ERROR_NO_CALLBACK_ACTIVE));
        break;
    }

    return S_OK;
}

// Static function to read file stream and pass data to FFmpeg. Credit to Philipp Sch http://www.codeproject.com/Tips/489450/Creating-Custom-FFmpeg-IO-Context
_Use_decl_annotations_
static int FileStreamRead(void* ptr, uint8_t* buf, int bufSize)
{
    IStream* pStream = reinterpret_cast<IStream*>(ptr);
    ULONG bytesRead = 0;
    HRESULT hr = pStream->Read(buf, bufSize, &bytesRead);

    if (FAILED(hr))
    {
        return -1;
    }

    // If we succeed but don't have any bytes, assume end of file
    if (bytesRead == 0)
    {
        return AVERROR_EOF;  // Let FFmpeg know that we have reached eof
    }

    return bytesRead;
}

// Static function to seek in file stream. Credit to Philipp Sch http://www.codeproject.com/Tips/489450/Creating-Custom-FFmpeg-IO-Context
_Use_decl_annotations_
static int64_t FileStreamSeek(void* ptr, int64_t pos, int whence)
{
    IStream* pStream = reinterpret_cast<IStream*>(ptr);
    LARGE_INTEGER in;
    in.QuadPart = pos;
    ULARGE_INTEGER out = { 0 };

    if (FAILED(pStream->Seek(in, whence, &out)))
    {
        return -1;
    }

    return out.QuadPart; // Return the new position:
}

#endif // NO_FFMPEG