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

#include "FFmpegReader.h"

using namespace FFmpegInterop;

_Use_decl_annotations_
FFmpegReader::FFmpegReader(AVFormatContext* avFormatCtx)
    : m_pAvFormatCtx(avFormatCtx)
    , m_audioStreamIndex(AVERROR_STREAM_NOT_FOUND)
    , m_videoStreamIndex(AVERROR_STREAM_NOT_FOUND)
{
}

_Use_decl_annotations_
FFmpegReader::~FFmpegReader()
{
}

// Read the next packet from the stream and push it into the appropriate
// sample provider
_Use_decl_annotations_
int FFmpegReader::ReadPacket()
{
    int ret;
    AVPacket avPacket;
    av_init_packet(&avPacket);
    avPacket.data = NULL;
    avPacket.size = 0;

    ret = av_read_frame(m_pAvFormatCtx, &avPacket);
    if (ret < 0)
    {
        return ret;
    }

    // Push the packet to the appropriate
    if (avPacket.stream_index == m_audioStreamIndex)
    {
        auto locked = m_audioSampleProvider.lock();
        if (locked != nullptr)
            locked->QueuePacket(avPacket);
    }
    else if (avPacket.stream_index == m_videoStreamIndex)
    {
        auto locked = m_videoSampleProvider.lock();
        if (locked != nullptr)
            locked->QueuePacket(avPacket);
    }
    else
    {
        DebugMessage(L"Ignoring unused stream\n");
        av_packet_unref(&avPacket);
    }

    return ret;
}

_Use_decl_annotations_
void FFmpegReader::SetAudioStream(int audioStreamIndex, std::weak_ptr<MediaSampleProvider> audioSampleProvider)
{
    m_audioStreamIndex = audioStreamIndex;
    m_audioSampleProvider = audioSampleProvider;

    auto locked = m_audioSampleProvider.lock();
    if (locked != nullptr)
    {
        locked->SetCurrentStreamIndex(m_audioStreamIndex);
    }
}

_Use_decl_annotations_
void FFmpegReader::SetVideoStream(int videoStreamIndex, std::weak_ptr<MediaSampleProvider> videoSampleProvider)
{
    m_videoStreamIndex = videoStreamIndex;
    m_videoSampleProvider = videoSampleProvider;

    auto locked = m_videoSampleProvider.lock();
    if (locked != nullptr)
    {
        locked->SetCurrentStreamIndex(m_videoStreamIndex);
    }
}

#endif // NO_FFMPEG