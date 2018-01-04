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

#ifndef CDEMUX_H
#define CDEMUX_H

#include "CStreamInfo.h"

class CDemux {
public:
    CDemux() {}
    virtual ~CDemux() {}
    /**
     * @param format,pDict Extra informations for demux
     * @return 0 for success.
     */
    virtual int start(const char *format = nullptr, AVDictionary *pDict = nullptr) { return 0; }
    /**
     * @return the number of streams on succuss;
     * otherwise a negative value.
     */
    virtual int getNumStreams() { return 0; }
    /**
     * @return the AVCodecPar of the streams[idx] on succuss,
     * otherwise null pointer.
     */
    virtual CStreamInfo* getStreamInfo(int strIdx) { return nullptr; }
    /**
     * @return 0 if OK, otherwise a negative value.
     */
    virtual int readPacket(AVPacket *avpkt) { return 0; }
    /**
     * The same as fseek/ftell, only workable when file streams.
     */
    virtual int seek(long offset, int whence) { return 0; }
    virtual int tell() { return 0; }
    virtual int size() { return 0; }
};

#endif /* CDEMUX_H */
