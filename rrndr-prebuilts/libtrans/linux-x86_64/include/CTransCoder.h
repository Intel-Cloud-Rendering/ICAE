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

#ifndef CTRANSCODER_H
#define CTRANSCODER_H

#include <pthread.h>
#include <string>
#include <map>
#include "CDemux.h"
#include "CMux.h"
#include "CRunable.h"

class CTransLog;
class CThread;
class CDecoder;
class CFilter;
class CEncoder;

class CTransCoder : public CRunable {
public:
    /**
     * Common Constructor.
     * @param sSrcUrl    input file or url
     * @param sDstUrl    output file or url
     * @param sDstFormat output format. This is necessary if no muxer can be determined
     *                   from the sDstUrl.
     */
    CTransCoder(std::string sSrcUrl, std::string sDstUrl, std::string sDstFormat = "");
    /**
     * Constructor with user-defined demuxer
     * @param sSrcUrl
     * @param pMux    user-defined muxer object.
     * Note: CTransCoder will take control of this object, so use 'new' to
     * allocate this object.
     */
    CTransCoder(std::string sSrcUrl, CMux *pMux);
    CTransCoder(CDemux *pDemux, CMux *pMux);
    CTransCoder(CDemux *pDemux, std::string sDstUrl, std::string sDstFormat = "");
    ~CTransCoder();
    /* Control functions */
    /**
     * Start the process.
     * @return 0 on success, otherwise fails.
     */
    int start();
    /**
     * Wait the transcoder task to finish.
     * That is, the process only terminates when coming accross EOF or error.
     */
    void wait();
    /**
     * Force to stop the procedure.
     */
    void stop();
    /**
     * Set properties for demuxer/muxer
     * @param key      the property to be set
     * @param value
     * @return 0 on success.
     * 
     * @Note: For common demuxers, these properties may be ignored since all necessary
     * information can be found by probing the input stream. But for demuxers such as
     * 'rawvideo', 'width'/'height'/'pixel_format' must be set, or the program will crash.
     * For muxers, supported options are 'w', 'h', 'pix_fmt', 'framerate', 'c', 'codec',
     * 'g', 'b' and some other options which FFmpeg supports.
     */
    int setInputProp(const char *key, const char *value);
    int setOutputProp(const char *key, const char *value);
    /* Inherrit runtime functions */
    void run();
    bool interrupt_callback();

private:
    /* Disable copy constructor and = */
    CTransCoder(const CTransCoder& orig);
    CTransCoder& operator=(const CTransCoder& orig);
    void interrupt();
    bool allStreamFound();
    int processInput();
    int newInputStream(int strIdx);
    int decode(int strIdx);
    int doOutput(bool flush);
    const char* getInOptVal(const char *short_name, const char *long_name,
                            const char *default_value = nullptr);
    const char* getOutOptVal(const char *short_name, const char *long_name,
                            const char *default_value = nullptr);

private:
    CDemux                   *m_pDemux;
    std::map<int, CDecoder *> m_mDecoders;
    std::map<int, CFilter *>  m_mFilters;
    std::map<int, CEncoder *> m_mEncoders;
    CMux                     *m_pMux;

    std::map<int, bool>       m_mStreamFound;
    volatile bool             m_bInterrupt;
    CThread                  *m_pThread;
    CTransLog                *m_Log;
    pthread_mutex_t           m_IntMutex;
    AVDictionary             *m_pInProp, *m_pOutProp;
};

#endif /* CTRANSCODER_H */
