#include "muduo20/co-net/Channel.h"

#include "muduo20/base/Logging.h"
#include "muduo20/base/Timestamp.h"
#include "muduo20/co-net/EventLoop.h"

#include <cassert>
#include <memory>
#include <poll.h>
#include <string>
#include <sys/poll.h>

using namespace muduo20;
using namespace muduo20::co_net;

const int Channel::kReadEvent = POLLIN | POLLPRI;
const int Channel::kWriteEvent = POLLOUT;

Channel::Channel(EventLoop *loop, int fd)
    : loop_(loop), fd_(fd), events_(0), revents_(0), index_(-1), logHup_(true),
      tied_(false), eventHandling_(false), addedToLoop_(false) {}

Channel::~Channel() {
    assert(!eventHandling_);
    assert(!addedToLoop_);
}

void Channel::tie(const std::shared_ptr<void> &obj) {
    tie_ = obj;
    tied_ = true;
}

void Channel::update() {
    addedToLoop_ = true;
    loop_->updateChannel(this);
}

void Channel::remove() {
    assert(isNoneEvent());
    addedToLoop_ = false;
    loop_->removeChannel(this);
}

void Channel::handleEvent(Timestamp receiveTime) {
    std::shared_ptr<void> guard;
    if (tied_) {
        guard = tie_.lock();
        if (guard) {
            handleEventWithGuard(receiveTime);
        }
        return;
    }
    handleEventWithGuard(receiveTime);
}

void Channel::handleEventWithGuard(Timestamp receiveTime) {
    eventHandling_ = true;
    muduo20::logTrace("{}", reventsToString());

    const bool hasInvalidEvent = (revents_ & POLLNVAL) != 0;
    if (((revents_ & POLLHUP) != 0) && ((revents_ & POLLIN) == 0)) {
        if (logHup_) {
            muduo20::logWarn("fd = {} Channel::handleEvent() POLLHUP", fd_);
        }
        if (closeCallback_) {
            closeCallback_();
        }
    }

    if (hasInvalidEvent) {
        muduo20::logWarn("fd = {} Channel::handleEvent() POLLNVAL", fd_);
    }

    if (((revents_ & POLLERR) != 0) || hasInvalidEvent) {
        if (errorCallback_) {
            errorCallback_();
        }
    }

    if ((revents_ & (POLLIN | POLLPRI | POLLRDHUP)) != 0) {
        if (readCallback_) {
            readCallback_(receiveTime);
        }
    }

    if ((revents_ & POLLOUT) != 0) {
        if (writeCallback_) {
            writeCallback_();
        }
    }

    eventHandling_ = false;
}

void Channel::enableReading() {
    events_ |= kReadEvent;
    update();
}

void Channel::disableReading() {
    events_ &= ~kReadEvent;
    update();
}

void Channel::enableWriting() {
    events_ |= kWriteEvent;
    update();
}

void Channel::disableWriting() {
    events_ &= ~kWriteEvent;
    update();
}

void Channel::disableAll() {
    events_ = kNoneEvent;
    update();
}

std::string Channel::reventsToString() const {
    return eventsToString(fd_, revents_);
}

std::string Channel::eventsToString() const {
    return eventsToString(fd_, events_);
}

std::string Channel::eventsToString(int fd, int events) {
    std::string result;
    result.reserve(48);
    result.append(std::to_string(fd));
    result.append(": ");
    if ((events & POLLIN) != 0) {
        result.append("IN ");
    }
    if ((events & POLLPRI) != 0) {
        result.append("PRI ");
    }
    if ((events & POLLOUT) != 0) {
        result.append("OUT ");
    }
    if ((events & POLLHUP) != 0) {
        result.append("HUP ");
    }
    if ((events & POLLRDHUP) != 0) {
        result.append("RDHUP ");
    }
    if ((events & POLLERR) != 0) {
        result.append("ERR ");
    }
    if ((events & POLLNVAL) != 0) {
        result.append("NVAL ");
    }
    return result;
}
