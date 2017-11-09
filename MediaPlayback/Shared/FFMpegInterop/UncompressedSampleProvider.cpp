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

#include "UncompressedSampleProvider.h"

using namespace FFmpegInterop;
using namespace ABI::Windows::Storage::Streams;

_Use_decl_annotations_
UncompressedSampleProvider::UncompressedSampleProvider(std::weak_ptr<FFmpegReader> reader, AVFormatContext* avFormatCtx, AVCodecContext* avCodecCtx)
    : MediaSampleProvider(reader, avFormatCtx, avCodecCtx)
    , m_pAvFrame(nullptr)
{
}

_Use_decl_annotations_
UncompressedSampleProvider::~UncompressedSampleProvider()
{

}

_Use_decl_annotations_
HRESULT UncompressedSampleProvider::ProcessDecodedFrame(IDataWriter* dataWriter)
{
    return S_OK;
}

// Return S_FALSE for an incomplete frame
_Use_decl_annotations_
HRESULT UncompressedSampleProvider::GetFrameFromFFmpegDecoder(AVPacket* avPacket)
{
    HRESULT hr = S_OK;
    int decodeFrame = 0;

    if (avPacket != nullptr)
    {
        int sendPacketResult = avcodec_send_packet(m_pAvCodecCtx, avPacket);
        if (sendPacketResult == AVERROR(EAGAIN))
        {
            // The decoder should have been drained and always ready to access input
            _ASSERT(FALSE);
            hr = E_UNEXPECTED;
        }
        else if (sendPacketResult < 0)
        {
            // We failed to send the packet
            hr = E_FAIL;
            DebugMessage(L"Decoder failed on the sample\n");
        }
    }
    if (SUCCEEDED(hr))
    {
        AVFrame *pFrame = av_frame_alloc();
        // Try to get a frame from the decoder.
        decodeFrame = avcodec_receive_frame(m_pAvCodecCtx, pFrame);

        // The decoder is empty, send a packet to it.
        if (decodeFrame == AVERROR(EAGAIN))
        {
            // The decoder doesn't have enough data to produce a frame,
            // return S_FALSE to indicate a partial frame
            hr = S_FALSE;
            av_frame_unref(pFrame);
            av_frame_free(&pFrame);
        }
        else if (decodeFrame < 0)
        {
            hr = E_FAIL;
            av_frame_unref(pFrame);
            av_frame_free(&pFrame);
            DebugMessage(L"Failed to get a frame from the decoder\n");
        }
        else
        {
            m_pAvFrame = pFrame;
        }
    }

    return hr;
}

_Use_decl_annotations_
HRESULT UncompressedSampleProvider::DecodeAVPacket(IDataWriter* dataWriter, AVPacket* avPacket, int64_t& framePts, int64_t& frameDuration)
{
    HRESULT hr = S_OK;
    bool fGotFrame = false;
    AVPacket *pPacket = avPacket;

    while (SUCCEEDED(hr))
    {
        hr = GetFrameFromFFmpegDecoder(pPacket);
        pPacket = nullptr;
        if (SUCCEEDED(hr))
        {
            if (hr == S_FALSE)
            {
                // If the decoder didn't give an initial frame we still need
                // to feed it more frames. Keep S_FALSE as the result
                if (fGotFrame)
                {
                    hr = S_OK;
                }
                break;
            }
            // Update the timestamp if the packet has one
            else if (m_pAvFrame->pts != AV_NOPTS_VALUE)
            {
                framePts = m_pAvFrame->pts;
                frameDuration = m_pAvFrame->pkt_duration;
            }
            fGotFrame = true;

            hr = ProcessDecodedFrame(dataWriter);
        }
    }

    return hr;
}


#endif // NO_FFMPEG