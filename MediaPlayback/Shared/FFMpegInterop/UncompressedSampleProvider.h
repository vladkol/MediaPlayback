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
#include "MediaSampleProvider.h"

extern "C"
{
#include <libswresample/swresample.h>
}

namespace FFmpegInterop
{
    class UncompressedSampleProvider 
        : public MediaSampleProvider
    {
    public:
        UncompressedSampleProvider(
            _In_ std::weak_ptr<FFmpegReader> reader,
            _In_ AVFormatContext* avFormatCtx,
            _In_ AVCodecContext* avCodecCtx);
        virtual ~UncompressedSampleProvider();

        // Try to get a frame from FFmpeg, otherwise, feed a frame to start decoding
        virtual HRESULT GetFrameFromFFmpegDecoder(
            _In_ AVPacket* avPacket);
        virtual HRESULT DecodeAVPacket(
            _In_ ABI::Windows::Storage::Streams::IDataWriter* dataWriter, 
            _In_ AVPacket* avPacket, 
            _Inout_ int64_t& framePts,
            _Inout_ int64_t& frameDuration) override;
        virtual HRESULT ProcessDecodedFrame(
            _In_ ABI::Windows::Storage::Streams::IDataWriter* dataWriter);

    protected:
        AVFrame* m_pAvFrame;
    };
}

