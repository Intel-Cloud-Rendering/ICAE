
#include "RemoteInputDataHandler.h"


namespace android {
namespace remoteinput {




/* Describes state of a multi-touch pointer  */
typedef struct MTSPointerState {
    /* Tracking ID assigned to the pointer by an app emulating multi-touch. */
    int tracking_id;
    /* X - coordinate of the tracked pointer. */
    int x;
    /* Y - coordinate of the tracked pointer. */
    int y;
    /* Current pressure value. */
    int pressure;
} MTSPointerState;

/* Maximum number of pointers, supported by multi-touch emulation. */
#define MTS_POINTERS_NUM    10
/* Signals that pointer is not tracked (or is "up"). */
#define MTS_POINTER_UP      -1
/* Special tracking ID for a mouse pointer. */
#define MTS_POINTER_MOUSE   -2

MTSPointerState tracked_pointers[MTS_POINTERS_NUM];
int tracked_pointer_num = 0;

/* Gets an index in the MTS's tracking pointers array MTS for the given
 * tracking id.
 * Return:
 *  Index of a matching entry in the MTS's tracking pointers array, or -1 if
 *  matching entry was not found.
 */
static int
_mtsstate_get_pointer_index(const MTSPointerState* pointerStates, int tracking_id)
{
    int index;
    for (index = 0; index < MTS_POINTERS_NUM; index++) {
        if (pointerStates[index].tracking_id == tracking_id) {
            return index;
        }
    }
    return -1;
}

/* Gets an index of the first untracking pointer in the MTS's tracking pointers
 * array.
 * Return:
 *  An index of the first untracking pointer, or -1 if all pointers are tracked.
 */
static int
_mtsstate_get_available_pointer_index(const MTSPointerState* pointerStates)
{
    return _mtsstate_get_pointer_index(pointerStates, MTS_POINTER_UP);
}

static void
_mtsstate_init_pointer()
{
    int index;
    for (index = 0; index < MTS_POINTERS_NUM; index++) {
        tracked_pointers[index].tracking_id = MTS_POINTER_UP;
    }

    tracked_pointer_num = 0;
}


void RemoteInputDataHandler::StartHandler() {
        mIsWorking = true;

        mEpollFD = ::epoll_create(MAX_HANDLE_FD_SIZE);
        
        start();
    }

    void RemoteInputDataHandler::StopHandler() {
        mIsWorking = false;
        wait();

        if (mEpollFD > 0)
            ::close(mEpollFD);
    }

    void RemoteInputDataHandler::removeConnection(int fd) {
        ::epoll_ctl(mEpollFD, EPOLL_CTL_DEL, fd, NULL);
        
        AutoLock lock(mListLock);
        mConnectionList.erase(fd);
    }

    bool RemoteInputDataHandler::addConnection(int socket) {
        //only one connection allowed in this remote input case.
        if (mConnectionList.size() > 0)
            return false;

        _mtsstate_init_pointer();
        
        std::shared_ptr<RemoteInputDataConnection> connection = 
                std::make_shared<RemoteInputDataConnection>(socket);
        
        AutoLock lock(mListLock);
        mConnectionList.insert(std::make_pair(socket, connection));
        lock.unlock();
        
        struct epoll_event ev;
        ev.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
        ev.data.fd = socket;
        ::epoll_ctl(mEpollFD, EPOLL_CTL_ADD, socket, &ev);

        return true;
    }

    void RemoteInputDataHandler::modConnection(int fd, bool askRead) {
        if (fd <= 0)
            return;
        
        struct epoll_event ev;
        ev.data.fd = fd;
        if (askRead)
            ev.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
        else
            ev.events = EPOLLOUT | EPOLLRDHUP;

        ::epoll_ctl(mEpollFD, EPOLL_CTL_MOD, fd, &ev);
    }

    void RemoteInputDataHandler::setUserEventAgent(const AndroidConsoleAgents* agents) {
        mUserEventAgent = (QAndroidUserEventAgent*)agents->user_event;
    }

    void RemoteInputDataHandler::consumeInputPacket(RemoteInputPacket * inputPacket) {
        const int slot_index = _mtsstate_get_pointer_index(tracked_pointers, inputPacket->tracking_id);
        if (slot_index < 0) {
            if (inputPacket->event == MOUSE_EVENT_DOWN) {

                mLastTimestamp = inputPacket->timestamp;

                struct timeval curtimeval;
                gettimeofday(&curtimeval, NULL);
                uint64_t curtiming = curtimeval.tv_sec * 1000 * 1000 + curtimeval.tv_usec;
                mLastSentTiming = curtiming;

                int slot_avail_index = _mtsstate_get_available_pointer_index(tracked_pointers);
 
                tracked_pointers[slot_avail_index].tracking_id = inputPacket->tracking_id;
                tracked_pointers[slot_avail_index].x = inputPacket->x;
                tracked_pointers[slot_avail_index].y = inputPacket->y;
                tracked_pointers[slot_avail_index].pressure = 1;

                tracked_pointer_num ++;

                if (tracked_pointer_num == 1) {
                    //first finger
                    mUserEventAgent->sendMouseEvent(inputPacket->x, inputPacket->y, 0, 1);
	            } else if (tracked_pointer_num == 2) {
	                //secondary finger
	                mUserEventAgent->sendMouseEvent(inputPacket->x, inputPacket->y, 0, 5);
	            } else {
	                assert(0);
	            }
            } else {
                assert(0);
                return;
            }

            printf("mLastTimestamp = %lld, mLastSentTiming = %lld\n", (long long int)mLastTimestamp, (long long int)mLastSentTiming);
        } else {
			uint64_t needsleeptime = 0;
            struct timeval curtimeval;
            gettimeofday(&curtimeval, NULL);
            uint64_t curtiming = curtimeval.tv_sec * 1000 * 1000 + curtimeval.tv_usec;
            
            if (mLastSentTiming != 0 && mLastTimestamp != 0) {
                
                uint64_t senttiming_offset = 0;
                if (curtiming > mLastSentTiming) {
                    senttiming_offset = curtiming - mLastSentTiming;
                }
                uint64_t timestamp_offset = 0;
                if (inputPacket->timestamp > mLastTimestamp){
                    timestamp_offset = inputPacket->timestamp - mLastTimestamp;
                }

                if (timestamp_offset > senttiming_offset) {
                    needsleeptime = timestamp_offset - senttiming_offset;
                    usleep(needsleeptime);
                }
            }

            if (inputPacket->event == MOUSE_EVENT_UP) {
                tracked_pointer_num --;
                tracked_pointers[slot_index].tracking_id = MTS_POINTER_UP;
                tracked_pointers[slot_index].x = 0;
                tracked_pointers[slot_index].y = 0;
                tracked_pointers[slot_index].pressure = 0;

                if (tracked_pointer_num == 0) {

                    mLastTimestamp = 0;
                    mLastSentTiming = 0;
                    //first finger
                    mUserEventAgent->sendMouseEvent(inputPacket->x, inputPacket->y, 0, 0);
                } else if (tracked_pointer_num == 1) {
                    //secondary finger
                    mLastTimestamp = inputPacket->timestamp;
		            mLastSentTiming = curtiming + needsleeptime;
                    
                    mUserEventAgent->sendMouseEvent(inputPacket->x, inputPacket->y, 0, 4);
                } else {
                    assert(0);
                }
            } else if (inputPacket->event == MOUSE_EVENT_MOVE) {
                tracked_pointers[slot_index].x = inputPacket->x;
                tracked_pointers[slot_index].y = inputPacket->y;

                mLastTimestamp = inputPacket->timestamp;
	            mLastSentTiming = curtiming + needsleeptime;
                
                mUserEventAgent->sendMouseEvent(inputPacket->x, inputPacket->y, 0, 1);
            } else {
                assert(0);
            }

            printf("mLastTimestamp = %lld, mLastSentTiming = %lld\n", (long long int)mLastTimestamp, (long long int)mLastSentTiming);
        }
    }
}
}




