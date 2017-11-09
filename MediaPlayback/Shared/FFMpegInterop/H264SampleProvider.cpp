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

#include "pch.h"

#ifndef NO_FFMPEG

#include "H264SampleProvider.h"

using namespace FFmpegInterop;
using namespace ABI::Windows::Storage::Streams;

_Use_decl_annotations_
H264SampleProvider::H264SampleProvider(
    std::weak_ptr<FFmpegReader> reader,
    AVFormatContext* avFormatCtx,
    AVCodecContext* avCodecCtx)
    : MediaSampleProvider(reader, avFormatCtx, avCodecCtx)
{
}

_Use_decl_annotations_
H264SampleProvider::~H264SampleProvider()
{
}

_Use_decl_annotations_
HRESULT H264SampleProvider::WriteAVPacketToStream(IDataWriter* dataWriter, AVPacket* avPacket)
{
    HRESULT hr = S_OK;
    // On a KeyFrame, write the SPS and PPS
    if (avPacket->flags & AV_PKT_FLAG_KEY)
    {
        hr = GetSPSAndPPSBuffer(dataWriter);
    }

    if (SUCCEEDED(hr))
    {
        // Call base class method that simply write the packet to stream as is
        hr = MediaSampleProvider::WriteAVPacketToStream(dataWriter, avPacket);
    }

    // We have a complete frame
    return hr;
}

_Use_decl_annotations_
HRESULT H264SampleProvider::GetSPSAndPPSBuffer(IDataWriter* dataWriter)
{
    HRESULT hr = S_OK;

    if (m_pAvCodecCtx->extradata == nullptr && m_pAvCodecCtx->extradata_size < 8)
    {
        // The data isn't present
        hr = E_FAIL;
    }
    else
    {
        // Write both SPS and PPS sequence as is from extradata
        //auto vSPSPPS = ref new Platform::Array<uint8_t>(m_pAvCodecCtx->extradata, m_pAvCodecCtx->extradata_size);
        dataWriter->WriteBytes(m_pAvCodecCtx->extradata_size, m_pAvCodecCtx->extradata);
    }

    return hr;
}

#endif // NO_FFMPEG