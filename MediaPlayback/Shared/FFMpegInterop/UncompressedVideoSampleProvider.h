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
#include <libswscale/swscale.h>
}


namespace FFmpegInterop
{
    class UncompressedVideoSampleProvider 
        : public UncompressedSampleProvider
    {
    public:
        UncompressedVideoSampleProvider(
            _In_ std::weak_ptr<FFmpegReader> reader,
            _In_ AVFormatContext* avFormatCtx,
            _In_ AVCodecContext* avCodecCtx);
        virtual ~UncompressedVideoSampleProvider();

        virtual HRESULT WriteAVPacketToStream(
            _In_ ABI::Windows::Storage::Streams::IDataWriter* writer, 
            _In_ AVPacket* avPacket) override;
        virtual HRESULT DecodeAVPacket(
            _In_ ABI::Windows::Storage::Streams::IDataWriter* dataWriter, 
            _In_ AVPacket* avPacket, 
            _Inout_ int64_t& framePts, 
            _Inout_ int64_t& frameDuration) override;
        virtual HRESULT AllocateResources() override;

    private:
        SwsContext* m_pSwsCtx;
        int m_rgVideoBufferLineSize[4];
        uint8_t* m_rgVideoBufferData[4];
    };
}

