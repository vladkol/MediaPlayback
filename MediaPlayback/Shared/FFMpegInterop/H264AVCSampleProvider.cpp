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

#include "H264AVCSampleProvider.h"

using namespace FFmpegInterop;
using namespace ABI::Windows::Storage::Streams;

_Use_decl_annotations_
H264AVCSampleProvider::H264AVCSampleProvider(
    std::weak_ptr<FFmpegReader> reader,
    AVFormatContext* avFormatCtx,
    AVCodecContext* avCodecCtx)
    : MediaSampleProvider(reader, avFormatCtx, avCodecCtx)
{
}

_Use_decl_annotations_
H264AVCSampleProvider::~H264AVCSampleProvider()
{
}

_Use_decl_annotations_
HRESULT H264AVCSampleProvider::WriteAVPacketToStream(IDataWriter* dataWriter, AVPacket* avPacket)
{
    HRESULT hr = S_OK;
    // On a KeyFrame, write the SPS and PPS
    if (avPacket->flags & AV_PKT_FLAG_KEY)
    {
        hr = GetSPSAndPPSBuffer(dataWriter);
    }

    if (SUCCEEDED(hr))
    {
        // Convert the packet to NAL format
        hr = WriteNALPacket(dataWriter, avPacket);
    }

    // We have a complete frame
    return hr;
}

_Use_decl_annotations_
HRESULT H264AVCSampleProvider::GetSPSAndPPSBuffer(IDataWriter* dataWriter)
{
    int spsLength = 0;
    int ppsLength = 0;

    // Get the position of the SPS
    if (m_pAvCodecCtx->extradata == nullptr && m_pAvCodecCtx->extradata_size < 8)
    {
        // The data isn't present
        IFR(E_FAIL);
    }

    byte* spsPos = m_pAvCodecCtx->extradata + 8;
    spsLength = spsPos[-1];

    if (m_pAvCodecCtx->extradata_size < (8 + spsLength))
    {
        // We don't have a complete SPS
        IFR(E_FAIL);
    }
    else
    {
        //auto vSPS = ref new Platform::Array<uint8_t>(spsPos, spsLength);

        // Write the NAL unit for the SPS
        dataWriter->WriteByte(0);
        dataWriter->WriteByte(0);
        dataWriter->WriteByte(0);
        dataWriter->WriteByte(1);

        // Write the SPS
        dataWriter->WriteBytes(spsLength, spsPos);
    }

    if (m_pAvCodecCtx->extradata_size < (8 + spsLength + 3))
    {
        IFR(E_FAIL);
    }

    byte* ppsPos = m_pAvCodecCtx->extradata + 8 + spsLength + 3;
    ppsLength = ppsPos[-1];

    if (m_pAvCodecCtx->extradata_size < (8 + spsLength + 3 + ppsLength))
    {
        IFR(E_FAIL);
    }
    else
    {
        //auto vPPS = ref new Platform::Array<uint8_t>(ppsPos, ppsLength);

        // Write the NAL unit for the PPS
        dataWriter->WriteByte(0);
        dataWriter->WriteByte(0);
        dataWriter->WriteByte(0);
        dataWriter->WriteByte(1);

        // Write the PPS
        dataWriter->WriteBytes(ppsLength, ppsPos);
    }

    return S_OK;
}

// Write out an H.264 packet converting stream offsets to start-codes
_Use_decl_annotations_
HRESULT H264AVCSampleProvider::WriteNALPacket(IDataWriter* dataWriter, AVPacket* avPacket)
{
    HRESULT hr = S_OK;
    uint32_t index = 0;
    uint32_t size = 0;
    uint32_t packetSize = (uint32_t)avPacket->size;

    do
    {
        // Make sure we have enough data
        if (packetSize < (index + 4))
        {
            hr = E_FAIL;
            break;
        }

        // Grab the size of the blob
        size = (avPacket->data[index] << 24) + (avPacket->data[index + 1] << 16) + (avPacket->data[index + 2] << 8) + avPacket->data[index + 3];

        // Write the NAL unit to the stream
        dataWriter->WriteByte(0);
        dataWriter->WriteByte(0);
        dataWriter->WriteByte(0);
        dataWriter->WriteByte(1);
        index += 4;

        // Stop if index and size goes beyond packet size or overflow
        if (packetSize < (index + size) || (UINT32_MAX - index) < size)
        {
            hr = E_FAIL;
            break;
        }

        // Write the rest of the packet to the stream
        //auto vBuffer = ref new Platform::Array<uint8_t>(&(avPacket->data[index]), size);
        dataWriter->WriteBytes(size, &avPacket->data[index]);
        index += size;
    } while (index < packetSize);

    return hr;
}

#endif // NO_FFMPEG