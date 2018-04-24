#pragma once

#include "RemoteInputDataConnection.h"

#include "android/base/threads/Thread.h"

#include "android/base/synchronization/Lock.h"
#include "android/base/synchronization/ConditionVariable.h"

#include "android/console.h"

#include <unistd.h>

#include <memory>
#include <unordered_map>

#include <list>
#include <sys/epoll.h>
#include <unistd.h>


using Lock = android::base::Lock;
using AutoLock = android::base::AutoLock;
using ConditionVariable = android::base::ConditionVariable;

#define MAX_HANDLE_FD_SIZE 100
namespace android {
namespace remoteinput {


typedef enum _MOUSE_EVENT_TYPE {
    MOUSE_EVENT_DOWN = 0,
    MOUSE_EVENT_UP,
    MOUSE_EVENT_MOVE
} MOUSE_EVENT_TYPE;

typedef struct _RemoteInputPacket  {
    int x : 16;
    int y : 16;
    MOUSE_EVENT_TYPE event : 8;
    int tracking_id : 32;
    uint64_t timestamp : 64;
} __attribute__ ((packed)) RemoteInputPacket;

#define REMOTE_INPUT_PACKET_LEN       (sizeof(RemoteInputPacket))


class RemoteInputDataHandler : public android::base::Thread {
public:
    RemoteInputDataHandler() :
        mUserEventAgent(NULL),
        mEpollFD(-1),
        mIsWorking(false),
        mLastTimestamp(0),
        mLastSentTiming(0)
         {};

    virtual intptr_t main() override {
        RemoteInputPacket curInputPacket;
        ssize_t packetStartPos = 0;

        struct epoll_event events[MAX_HANDLE_FD_SIZE];
        
        printf("remote input datahandler thread begins\n");
        while (mIsWorking) {
            //printf("connection count = %d-------------------\n", (int)mConnectionList.size());
            int ret = ::epoll_wait(mEpollFD, events, MAX_HANDLE_FD_SIZE, 100);

            for (int i = 0; i < ret; i ++) {
                if ((events[i].events & EPOLLERR) ||  
                      (events[i].events & EPOLLHUP) ||
                      (events[i].events & EPOLLRDHUP)) {
                      printf("found a connection exited\n");
                      mLastSentTiming = 0;
                      mLastTimestamp = 0;
                      removeConnection(events[i].data.fd);
                      continue;  
                } else if (events[i].events & EPOLLIN) {
                    AutoLock lock(mListLock);
                    auto it =
                        mConnectionList.find(events[i].data.fd);
                    if (it == mConnectionList.end()) {
                        continue;
                    }
                    
                    std::shared_ptr<RemoteInputDataConnection> connection = it->second;
                    
                    lock.unlock();

                    //mUserEventAgent->sendMouseEvent(483, 656, 0, 1);
                    //usleep(1000 * 20);
                    //mUserEventAgent->sendMouseEvent(472, 656, 0, 1);
                    //usleep(1000 * 20);
                    //mUserEventAgent->sendMouseEvent(450, 656, 0, 1);
                    //usleep(1000 * 20);
                    //mUserEventAgent->sendMouseEvent(450, 656, 0, 0);

                    //the loop will read off socket receive buffer because epollet is set.
                    while (1) {
                        ssize_t readSize = connection->receiveDataFromSocketNonBlocking(
                            (char*)(&curInputPacket) + packetStartPos, (ssize_t)REMOTE_INPUT_PACKET_LEN - packetStartPos);

                        if (readSize < 0)
                            break;
                        else if (readSize == (ssize_t)REMOTE_INPUT_PACKET_LEN - packetStartPos) {
                            
                            consumeInputPacket(&curInputPacket);
                            
                            packetStartPos = 0;
                            continue; // perhaps still data exist in receive buffer, try next packet
                        } else {
                            printf("not a complete packet, keep receiving\n");
                            // no data in receive buffer, exit the loop, wait for next epoll event
                            packetStartPos += readSize;
                            break;
                        }
                    }
                }
            }

            
        }

        printf("remote input datahandler thread exit\n");
        return 0;
    }

    void StartHandler();

    void StopHandler();

    void removeConnection(int fd);

    bool addConnection(int socket);

    void modConnection(int fd, bool askRead);

    void setUserEventAgent(const AndroidConsoleAgents* agents);

    void consumeInputPacket(RemoteInputPacket * inputPacket);
    
private:
    QAndroidUserEventAgent * mUserEventAgent;
        
    int mEpollFD;
    
    bool mIsWorking;

    uint64_t mLastTimestamp;
    uint64_t mLastSentTiming;

    std::unordered_map<int, std::shared_ptr<RemoteInputDataConnection> > mConnectionList;
    
    Lock mListLock;
};

}}
