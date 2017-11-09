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

#include "UncompressedAudioSampleProvider.h"

using namespace FFmpegInterop;
using namespace ABI::Windows::Storage::Streams;

_Use_decl_annotations_
UncompressedAudioSampleProvider::UncompressedAudioSampleProvider(
    std::weak_ptr<FFmpegReader> reader,
    AVFormatContext* avFormatCtx,
    AVCodecContext* avCodecCtx)
    : UncompressedSampleProvider(reader, avFormatCtx, avCodecCtx)
    , m_pSwrCtx(nullptr)
{
}

_Use_decl_annotations_
UncompressedAudioSampleProvider::~UncompressedAudioSampleProvider()
{
    if (m_pAvFrame)
    {
        av_frame_free(&m_pAvFrame);
    }

    // Free 
    swr_free(&m_pSwrCtx);
}

_Use_decl_annotations_
HRESULT UncompressedAudioSampleProvider::AllocateResources()
{
    HRESULT hr = S_OK;
    hr = UncompressedSampleProvider::AllocateResources();
    if (SUCCEEDED(hr))
    {
        // Set default channel layout when the value is unknown (0)
        int64_t inChannelLayout = m_pAvCodecCtx->channel_layout ? m_pAvCodecCtx->channel_layout : av_get_default_channel_layout(m_pAvCodecCtx->channels);
        int64_t outChannelLayout = av_get_default_channel_layout(m_pAvCodecCtx->channels);

        // Set up resampler to convert any PCM format (e.g. AV_SAMPLE_FMT_FLTP) to AV_SAMPLE_FMT_S16 PCM format that is expected by Media Element.
        // Additional logic can be added to avoid resampling PCM data that is already in AV_SAMPLE_FMT_S16_PCM.
        m_pSwrCtx = swr_alloc_set_opts(
            NULL,
            outChannelLayout,
            AV_SAMPLE_FMT_S16,
            m_pAvCodecCtx->sample_rate,
            inChannelLayout,
            m_pAvCodecCtx->sample_fmt,
            m_pAvCodecCtx->sample_rate,
            0,
            NULL);

        if (!m_pSwrCtx)
        {
            hr = E_OUTOFMEMORY;
        }
    }

    if (SUCCEEDED(hr))
    {
        if (swr_init(m_pSwrCtx) < 0)
        {
            hr = E_FAIL;
        }
    }

    return hr;
}

_Use_decl_annotations_
HRESULT UncompressedAudioSampleProvider::WriteAVPacketToStream(IDataWriter* dataWriter, AVPacket* avPacket)
{
    // Because each packet can contain multiple frames, we have already written the packet to the stream
    // during the decode stage.
    return S_OK;
}

_Use_decl_annotations_
HRESULT UncompressedAudioSampleProvider::ProcessDecodedFrame(IDataWriter* dataWriter)
{
    // Resample uncompressed frame to AV_SAMPLE_FMT_S16 PCM format that is expected by Media Element
    uint8_t *resampledData = nullptr;
    unsigned int aBufferSize = av_samples_alloc(&resampledData, NULL, m_pAvFrame->channels, m_pAvFrame->nb_samples, AV_SAMPLE_FMT_S16, 0);
    int resampledDataSize = swr_convert(m_pSwrCtx, &resampledData, aBufferSize, (const uint8_t **)m_pAvFrame->extended_data, m_pAvFrame->nb_samples);
    //auto aBuffer = ref new Platform::Array<uint8_t>(resampledData, min(aBufferSize, (unsigned int)(resampledDataSize * m_pAvFrame->channels * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16))));
    dataWriter->WriteBytes(min(aBufferSize, (unsigned int)(resampledDataSize * m_pAvFrame->channels * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16))), resampledData);
    av_freep(&resampledData);
    av_frame_unref(m_pAvFrame);
    av_frame_free(&m_pAvFrame);

    return S_OK;
}

#endif // NO_FFMPEG