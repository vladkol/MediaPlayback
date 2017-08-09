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
