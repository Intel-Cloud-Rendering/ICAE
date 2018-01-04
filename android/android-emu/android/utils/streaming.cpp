
#include "android/base/synchronization/Lock.h"
#include "streaming.h"
#include "CTransCoder.h"
#include <inttypes.h>

ANDROID_BEGIN_HEADER
#include "libavutil/samplefmt.h"
#include "libavutil/time.h"
#include "libavutil/fifo.h"
ANDROID_END_HEADER

enum {
    AUD_FMT_U8,
    AUD_FMT_S8,
    AUD_FMT_U16,
    AUD_FMT_S16,
    AUD_FMT_U32,
    AUD_FMT_S32
};

class IRRAudioDemuxer : public CDemux {
public:
    IRRAudioDemuxer(int channels, int freq, int fmt) {
        m_Info.m_rTimeBase  = AV_TIME_BASE_Q;
        m_Info.m_rFrameRate = (AVRational){freq, 1};
        m_Info.m_pCodecPars->sample_rate = freq;
        m_Info.m_pCodecPars->channels    = channels;
        m_Info.m_pCodecPars->format      = mapFormat(fmt);
        m_Info.m_pCodecPars->codec_id    = findCodec(fmt);
        m_Info.m_pCodecPars->codec_type  = AVMEDIA_TYPE_AUDIO;
        m_Info.m_pCodecPars->bit_rate    = 256*1000;

        /* The number of samples in one single audio packet */
        m_nSamples   = 1024;
        /* The number of bytes 1 sample needed */
        m_nBps       = av_get_bytes_per_sample((AVSampleFormat)m_Info.m_pCodecPars->format);
        m_nBps      *= channels;
        m_nMaxSize   = av_samples_get_buffer_size(nullptr,
                                                  m_Info.m_pCodecPars->channels, m_nSamples,
                                                 (AVSampleFormat)m_Info.m_pCodecPars->format, 1);
        /* Cycle FIFO to store the incoming PCM data */
        m_pFifo      = av_fifo_alloc(m_nMaxSize * 10);
        m_nStartTime = av_gettime_relative();
        m_nCurPts    = 0;

        createMutePkt();
    }

    ~IRRAudioDemuxer() {
        av_packet_unref(&m_MutePkt);
        av_fifo_free(m_pFifo);
    }

    int getNumStreams() {
        return 1;
    }

    CStreamInfo* getStreamInfo(int strIdx) {
        return &m_Info;
    }

    int readPacket(AVPacket *avpkt) {
        int  ret = 0;
        int size = FFMIN(av_fifo_size(m_pFifo), m_nMaxSize);

        while (av_gettime_relative() - m_nStartTime <= m_nCurPts)
            av_usleep(100);

        android::base::AutoLock mutex(mLock);

        if (size > 0) {
            avpkt->data     = m_pFifo->rptr;
            avpkt->size     = FFMIN(size, m_pFifo->end - m_pFifo->rptr);
            avpkt->duration = getDuration(avpkt->size/m_nBps);
            av_fifo_drain(m_pFifo, avpkt->size);
        } else
            ret = av_packet_ref(avpkt, &m_MutePkt);

        avpkt->pts = avpkt->dts = m_nCurPts;
        m_nCurPts += avpkt->duration;

        return ret;
    }

    int write(void *buf, int size) {
        android::base::AutoLock mutex(mLock);
        return av_fifo_generic_write(m_pFifo, buf, size, nullptr);
    }

private:
    mutable android::base::Lock mLock;
    CStreamInfo  m_Info;
    AVPacket     m_MutePkt;
    int          m_nSamples, m_nBps, m_nMaxSize;
    int64_t      m_nStartTime, m_nCurPts;
    AVFifoBuffer *m_pFifo;

    void createMutePkt() {
        int ret;

        av_init_packet(&m_MutePkt);

        ret = av_new_packet(&m_MutePkt, m_nMaxSize);
        if (ret < 0) {
            return;
        }

        memset(m_MutePkt.data, 0x00, m_nMaxSize);
        m_MutePkt.pts      = m_MutePkt.dts = AV_NOPTS_VALUE;
        m_MutePkt.duration = getDuration(m_nSamples);
    }

    AVSampleFormat mapFormat(int fmt) {
        switch (fmt) {
            case AUD_FMT_U8:
            case AUD_FMT_S8:
                return AV_SAMPLE_FMT_U8;
            case AUD_FMT_U16:
            case AUD_FMT_S16:
                return AV_SAMPLE_FMT_S16;
            case AUD_FMT_U32:
            case AUD_FMT_S32:
                return AV_SAMPLE_FMT_S32;
        }

        return AV_SAMPLE_FMT_NONE;
    }

    AVCodecID findCodec(int fmt) {
        switch (fmt) {
            case AUD_FMT_U8:
                return AV_CODEC_ID_PCM_U8;
            case AUD_FMT_S8:
                return AV_CODEC_ID_PCM_S8;
            case AUD_FMT_U16:
                return AV_CODEC_ID_PCM_U16LE;
            case AUD_FMT_S16:
                return AV_CODEC_ID_PCM_S16LE;
            case AUD_FMT_U32:
                return AV_CODEC_ID_PCM_U32LE;
            case AUD_FMT_S32:
                return AV_CODEC_ID_PCM_S32LE;
        }
        return AV_CODEC_ID_NONE;
    }

    int64_t getDuration(int nSamples) {
        AVRational r = (AVRational) {1, m_Info.m_pCodecPars->sample_rate};
        return av_rescale_q(nSamples, r, AV_TIME_BASE_Q);
    }
};

struct RRndrStream {
    IRRAudioDemuxer *pDemux;
    CTransCoder *pTrans;
};


RRndrStream *RRndr_create_audio(int channels, int freq, int fmt, const char *path) {
    RRndrStream *st = new RRndrStream();

    st->pDemux = new IRRAudioDemuxer(channels, freq, fmt);
    st->pTrans = new CTransCoder(st->pDemux, path);
    st->pTrans->start();

    return st;
}

void RRndr_delete(RRndrStream *stream) {
    if (!stream)
        return;

    stream->pTrans->stop();
    delete stream->pTrans;
    delete stream;
}

int RRndr_write(struct RRndrStream *stream, void *buf, int size) {
    return stream->pDemux->write(buf, size);
}
