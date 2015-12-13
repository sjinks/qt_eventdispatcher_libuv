#ifndef EVENTDISPATCHER_LIBUV_QPA_H
#define EVENTDISPATCHER_LIBUV_QPA_H

#include "eventdispatcher_libuv.h"

#if QT_VERSION < 0x050000
#	error This code requires at least Qt 5
#endif

class EventDispatcherLibUvQPA : public EventDispatcherLibUv {
	Q_OBJECT
public:
	explicit EventDispatcherLibUvQPA(QObject* parent = 0);
	virtual ~EventDispatcherLibUvQPA(void);

	bool processEvents(QEventLoop::ProcessEventsFlags flags) Q_DECL_OVERRIDE;
	bool hasPendingEvents(void) Q_DECL_OVERRIDE;
	void flush(void) Q_DECL_OVERRIDE;

private:
	Q_DISABLE_COPY(EventDispatcherLibUvQPA)
};

#endif // EVENTDISPATCHER_LIBUV_QPA_H
