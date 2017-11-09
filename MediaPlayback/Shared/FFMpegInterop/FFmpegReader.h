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

namespace FFmpegInterop
{
    class FFmpegReader 
        : public SharedFromThis
    {
    public:
        FFmpegReader(_In_ AVFormatContext* avFormatCtx);
        virtual ~FFmpegReader();
        int ReadPacket();
        void SetAudioStream(_In_ int audioStreamIndex, _In_ std::weak_ptr<MediaSampleProvider> audioSampleProvider);
        void SetVideoStream(_In_ int videoStreamIndex, _In_ std::weak_ptr<MediaSampleProvider> videoSampleProvider);

    private:
        AVFormatContext* m_pAvFormatCtx;

        int m_audioStreamIndex;
        std::weak_ptr<MediaSampleProvider> m_audioSampleProvider;

        int m_videoStreamIndex;
        std::weak_ptr<MediaSampleProvider> m_videoSampleProvider;
    };
}
