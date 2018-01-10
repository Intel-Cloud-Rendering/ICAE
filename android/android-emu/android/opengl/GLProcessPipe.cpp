// Copyright 2016 The Android Open Source Project
//
// This software is licensed under the terms of the GNU General Public
// License version 2, as published by the Free Software Foundation, and
// may be copied, distributed, and modified under those terms.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

#include "android/emulation/AndroidPipe.h"

#include "android/opengles.h"
#include <assert.h>
#include <atomic>
#include <memory>

// Set to 1 or 2 for debug traces
#define DEBUG 0

#if DEBUG >= 1
#define D(...) printf(__VA_ARGS__), printf("\n"), fflush(stdout)
#else
#define D(...) ((void)0)
#endif

#if DEBUG >= 2
#define DD(...) printf(__VA_ARGS__), printf("\n"), fflush(stdout)
#else
#define DD(...) ((void)0)
#endif

using ChannelBuffer = emugl::RenderChannel::Buffer;
using emugl::RenderChannel;
using emugl::RenderChannelPtr;

typedef struct szGLCommandPacketSetPuid {
    uint32_t flag;
    uint32_t opcode;
    uint32_t len;
    uint64_t puid;
} __attribute__ ((packed)) GLCommandPacketSetPuid;

namespace android {
namespace opengl {

namespace {

// GLProcessPipe is a pipe service that is used for releasing graphics resources
// per guest process. At the time being, guest processes can acquires host color
// buffer handles / EGLImage handles and they need to be properly released when
// guest process exits unexpectedly. This class is used to detect if guest
// process exits, so that a proper cleanup function can be called.

// It is done by setting up a pipe per guest process before acquiring color
// buffer handles. When guest process exits, the pipe will be closed, and
// onGuestClose() will trigger the cleanup path.

class GLProcessPipe : public AndroidPipe {
public:
    //////////////////////////////////////////////////////////////////////////
    // The pipe service class for this implementation.
    class Service : public AndroidPipe::Service {
    public:
        Service() : AndroidPipe::Service("GLProcessPipe") {}

        virtual AndroidPipe* create(void* mHwPipe, const char* args) override {
            auto renderer = android_getOpenglesRenderer();
            if (!renderer) {
                // This should never happen, unless there is a bug in the
                // emulator's initialization, or the system image.
                D("Trying to open the OpenGLES pipe without GPU emulation!");
                return nullptr;
            }
            
            return new GLProcessPipe(mHwPipe, this, renderer);
        }
    };

    GLProcessPipe(void* hwPipe, Service* service, 
        const emugl::RendererPtr& renderer) : AndroidPipe(hwPipe, service) {

        m_uniqueId = ++s_headId;

        mHasSetPuid = false;

        mChannel = renderer->createRenderChannel();
        if (!mChannel) {
            D("Failed to create an OpenGLES pipe channel!");
            return;
        }
    }

    void onGuestClose() override {
        // process died on the guest, cleanup gralloc memory on the host
        mChannel->stop();
        // Make sure there's no operation scheduled for this pipe instance to
        // run on the main thread.
        abortPendingOperation();
        delete this;
    }
    unsigned onGuestPoll() const override {
        return PIPE_POLL_IN | PIPE_POLL_OUT;
    }
    int onGuestRecv(AndroidPipeBuffer* buffers, int numBuffers) override {
        assert(buffers[0].size >= 8);
        memcpy(buffers[0].data, (const char*)&m_uniqueId, sizeof(m_uniqueId));
        return sizeof(m_uniqueId);
    }
    int onGuestSend(const AndroidPipeBuffer* buffers,
                            int numBuffers) override {

        //send the puid to remote render, so that the puid refcount ++ in remote
        ChannelBuffer outBuffer;
        outBuffer.resize_noinit(sizeof(GLCommandPacketSetPuid));
        GLCommandPacketSetPuid * ptr = (GLCommandPacketSetPuid*)(outBuffer.data());
        ptr->opcode = 10033;
        ptr->len = sizeof(GLCommandPacketSetPuid) - sizeof(GLCommandPacketSetPuid::flag);
        ptr->puid = m_uniqueId;

        // Send it through the channel.
        mChannel->tryWrite(std::move(outBuffer));
        
        // The guest is supposed to send us a confirm code first. The code is
        // 100 (4 byte integer).
        assert(buffers[0].size >= 4);
        int32_t confirmInt = *((int32_t*)buffers[0].data);
        assert(confirmInt == 100);
        (void)confirmInt;
        return buffers[0].size;
    }
    void onGuestWantWakeOn(int flags) override {}
private:
    // An identifier for the guest process corresponding to this pipe.
    // With very high probability, all currently-active processes have unique
    // identifiers, since the IDs are assigned sequentially from a 64-bit ID
    // space.
    // Please change it if you ever have a use case that exhausts them
    uint64_t m_uniqueId;
    static std::atomic_ullong s_headId;

    // A RenderChannel pointer used for communication.
    RenderChannelPtr mChannel;

    bool mHasSetPuid;
};

std::atomic_ullong GLProcessPipe::s_headId(0);

}

void registerGLProcessPipeService() {
    android::AndroidPipe::Service::add(new GLProcessPipe::Service());
}

}
}
