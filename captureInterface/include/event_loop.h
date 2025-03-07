#ifndef EVENT_LOOP_H
#define EVENT_LOOP_H

#include "event_data.h"
#include <mutex>
#include <vector>

class EventLoop {

public:
    static EventLoop* Instance();
    void Start();
    void AddEvent(Event& e);
    void ProcessEvent(Event& e);
    void End();

private:
    // Static pointer to the Singleton instance
    static EventLoop* instancePtr;
    static std::mutex instMutex;

    bool running;
    std::vector<Event> eventBuf;
    std::mutex eventBufMutex;

    EventLoop(){};
    
    // deleting the copy constructor to prevent copies
    EventLoop(const EventLoop& obj) = delete;
    void operator=(EventLoop const&) = delete;
};

#endif
