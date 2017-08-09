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

extern "C"
{
#include <libavformat/avformat.h>
}

namespace FFmpegInterop
{
    class FFmpegInteropMSS;
    class FFmpegReader;

    class MediaSampleProvider 
        : public SharedFromThis
    {
    public:
        MediaSampleProvider(
            _In_ std::weak_ptr<FFmpegReader> reader,
            _In_ AVFormatContext* avFormatCtx,
            _In_ AVCodecContext* avCodecCtx);
        virtual ~MediaSampleProvider();
        virtual HRESULT GetNextSample(
            _In_ ABI::Windows::Media::Core::IMediaStreamSample** ppSample);
        virtual void Flush();
        virtual void SetCurrentStreamIndex(
            _In_ int streamIndex);

        void QueuePacket(
            _In_ AVPacket packet);
        AVPacket PopPacket();
        virtual HRESULT AllocateResources();
        virtual HRESULT WriteAVPacketToStream(
            _In_ ABI::Windows::Storage::Streams::IDataWriter* writer, 
            _In_ AVPacket* avPacket);
        virtual HRESULT DecodeAVPacket(
            _In_ ABI::Windows::Storage::Streams::IDataWriter* dataWriter, 
            _In_ AVPacket* avPacket, 
            _Out_ int64_t& framePts, 
            _Out_ int64_t& frameDuration);

    private:
        std::vector<AVPacket> m_packetQueue;
        int m_streamIndex;
        Microsoft::WRL::ComPtr<ABI::Windows::Media::Core::IMediaStreamSampleStatics> spFactory;
        Microsoft::WRL::ComPtr<ABI::Windows::Storage::Streams::IDataWriterFactory> spDataWriterFactory;

    protected:
        // The FFmpeg context. Because they are complex types
        // we declare them as internal so they don't get exposed
        // externally
        std::weak_ptr<FFmpegReader> m_spReader;
        AVFormatContext* m_pAvFormatCtx;
        AVCodecContext* m_pAvCodecCtx;
    };
}