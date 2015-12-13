#include <qplatformdefs.h>
#include <qpa/qwindowsysteminterface.h>
#include <QtGui/QGuiApplication>
#include "eventdispatcher_libuv_qpa.h"

EventDispatcherLibUvQPA::EventDispatcherLibUvQPA(QObject* parent)
	: EventDispatcherLibUv(parent)
{
}

EventDispatcherLibUvQPA::~EventDispatcherLibUvQPA(void)
{
}

bool EventDispatcherLibUvQPA::processEvents(QEventLoop::ProcessEventsFlags flags)
{
	bool sent_events = QWindowSystemInterface::sendWindowSystemEvents(flags);

	if (EventDispatcherLibUv::processEvents(flags)) {
		return true;
	}

	return sent_events;
}

bool EventDispatcherLibUvQPA::hasPendingEvents(void)
{
	return EventDispatcherLibUv::hasPendingEvents() || QWindowSystemInterface::windowSystemEventsQueued();
}

void EventDispatcherLibUvQPA::flush(void)
{
	if (qApp) {
		qApp->sendPostedEvents();
	}
}
