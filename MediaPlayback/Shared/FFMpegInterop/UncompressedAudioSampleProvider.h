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
#include "UncompressedSampleProvider.h"

extern "C"
{
#include <libswresample/swresample.h>
}

namespace FFmpegInterop
{
    class UncompressedAudioSampleProvider 
        : UncompressedSampleProvider
    {
    public:
        UncompressedAudioSampleProvider(
            _In_ std::weak_ptr<FFmpegReader> reader,
            _In_ AVFormatContext* avFormatCtx,
            _In_ AVCodecContext* avCodecCtx);
        virtual ~UncompressedAudioSampleProvider();

        virtual HRESULT WriteAVPacketToStream(
            _In_ ABI::Windows::Storage::Streams::IDataWriter* writer, 
            _In_ AVPacket* avPacket) override;
        virtual HRESULT ProcessDecodedFrame(
            _In_ ABI::Windows::Storage::Streams::IDataWriter* dataWriter) override;
        virtual HRESULT AllocateResources() override;

    private:
        SwrContext* m_pSwrCtx;
    };
}

