/*
 * Copyright (C) 2017 Intel
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef CSTREAMINFO_H
#define CSTREAMINFO_H

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

struct CStreamInfo {
    CStreamInfo() {
        m_pCodecPars = avcodec_parameters_alloc();
        m_rFrameRate = (AVRational) {0, 1};
        m_rTimeBase  = AV_TIME_BASE_Q;
    }
    ~CStreamInfo() {
        avcodec_parameters_free(&m_pCodecPars);
    }
    CStreamInfo(const CStreamInfo &orig) {
        m_pCodecPars = avcodec_parameters_alloc();
        avcodec_parameters_copy(m_pCodecPars, orig.m_pCodecPars);
        m_rFrameRate = orig.m_rFrameRate;
        m_rTimeBase  = orig.m_rTimeBase;
    }

    CStreamInfo& operator=(const CStreamInfo &orig) {
        avcodec_parameters_copy(m_pCodecPars, orig.m_pCodecPars);
        m_rFrameRate = orig.m_rFrameRate;
        m_rTimeBase  = orig.m_rTimeBase;
        return *this;
    }

    AVCodecParameters *m_pCodecPars;
    AVRational         m_rFrameRate;
    AVRational         m_rTimeBase;
};

#endif /* CSTREAMINFO_H */

