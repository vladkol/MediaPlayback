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