#ifndef EVENTDISPATCHER_H
#define EVENTDISPATCHER_H

#include "eventdispatcher_libuv.h"

class EventDispatcher : public EventDispatcherLibUv {
	Q_OBJECT
public:
	explicit EventDispatcher(QObject* parent = 0) : EventDispatcherLibUv(parent) {}
};

#endif // EVENTDISPATCHER_H
