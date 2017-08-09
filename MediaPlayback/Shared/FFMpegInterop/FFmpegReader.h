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
