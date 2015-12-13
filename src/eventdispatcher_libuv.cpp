#include <QtCore/QPair>
#include <QtCore/QSocketNotifier>
#include <QtCore/QThread>
#include "eventdispatcher_libuv.h"
#include "eventdispatcher_libuv_p.h"

EventDispatcherLibUv::EventDispatcherLibUv(QObject* parent)
	: QAbstractEventDispatcher(parent), d_ptr(new EventDispatcherLibUvPrivate(this))
{
}

EventDispatcherLibUv::~EventDispatcherLibUv(void)
{
#if QT_VERSION < 0x040600
	delete this->d_ptr;
	this->d_ptr = 0;
#endif
}

bool EventDispatcherLibUv::processEvents(QEventLoop::ProcessEventsFlags flags)
{
	Q_D(EventDispatcherLibUv);
	return d->processEvents(flags);
}

bool EventDispatcherLibUv::hasPendingEvents(void)
{
	extern uint qGlobalPostedEventsCount();
	return qGlobalPostedEventsCount() > 0;
}

void EventDispatcherLibUv::registerSocketNotifier(QSocketNotifier* notifier)
{
#ifndef QT_NO_DEBUG
	if (notifier->socket() < 0) {
		qWarning("QSocketNotifier: Internal error: sockfd < 0");
		return;
	}

	if (notifier->thread() != thread() || thread() != QThread::currentThread()) {
		qWarning("QSocketNotifier: socket notifiers cannot be enabled from another thread");
		return;
	}
#endif

	if (notifier->type() == QSocketNotifier::Exception) {
		return;
	}

	Q_D(EventDispatcherLibUv);
	d->registerSocketNotifier(notifier);
}

void EventDispatcherLibUv::unregisterSocketNotifier(QSocketNotifier* notifier)
{
#ifndef QT_NO_DEBUG
	if (notifier->socket() < 0) {
		qWarning("QSocketNotifier: Internal error: sockfd < 0");
		return;
	}

	if (notifier->thread() != thread() || thread() != QThread::currentThread()) {
		qWarning("QSocketNotifier: socket notifiers cannot be disabled from another thread");
		return;
	}
#endif

	// Short circuit, we do not support QSocketNotifier::Exception
	if (notifier->type() == QSocketNotifier::Exception) {
		return;
	}

	Q_D(EventDispatcherLibUv);
	d->unregisterSocketNotifier(notifier);
}

void EventDispatcherLibUv::registerTimer(
	int timerId,
	int interval,
#if QT_VERSION >= 0x050000
	Qt::TimerType timerType,
#endif
	QObject* object
)
{
#ifndef QT_NO_DEBUG
	if (timerId < 1 || interval < 0 || !object) {
		qWarning("%s: invalid arguments", Q_FUNC_INFO);
		return;
	}

	if (object->thread() != this->thread() && this->thread() != QThread::currentThread()) {
		qWarning("%s: timers cannot be started from another thread", Q_FUNC_INFO);
		return;
	}
#endif

	Qt::TimerType type;
#if QT_VERSION >= 0x050000
	type = timerType;
#else
	type = Qt::CoarseTimer;
#endif

	Q_D(EventDispatcherLibUv);
	if (interval) {
		d->registerTimer(timerId, interval, type, object);
	}
	else {
		d->registerZeroTimer(timerId, object);
	}
}

bool EventDispatcherLibUv::unregisterTimer(int timerId)
{
#ifndef QT_NO_DEBUG
	if (timerId < 1) {
		qWarning("%s: invalid arguments", Q_FUNC_INFO);
		return false;
	}

	if (this->thread() != QThread::currentThread()) {
		qWarning("%s: timers cannot be stopped from another thread", Q_FUNC_INFO);
		return false;
	}
#endif

	Q_D(EventDispatcherLibUv);
	return d->unregisterTimer(timerId);
}

bool EventDispatcherLibUv::unregisterTimers(QObject* object)
{
#ifndef QT_NO_DEBUG
	if (!object) {
		qWarning("%s: invalid arguments", Q_FUNC_INFO);
		return false;
	}

	if (object->thread() != this->thread() && this->thread() != QThread::currentThread()) {
		qWarning("%s: timers cannot be stopped from another thread", Q_FUNC_INFO);
		return false;
	}
#endif

	Q_D(EventDispatcherLibUv);
	return d->unregisterTimers(object);
}

QList<QAbstractEventDispatcher::TimerInfo> EventDispatcherLibUv::registeredTimers(QObject* object) const
{
	if (!object) {
		qWarning("%s: invalid argument", Q_FUNC_INFO);
		return QList<QAbstractEventDispatcher::TimerInfo>();
	}

	Q_D(const EventDispatcherLibUv);
	return d->registeredTimers(object);
}

#if QT_VERSION >= 0x050000
int EventDispatcherLibUv::remainingTime(int timerId)
{
	Q_D(const EventDispatcherLibUv);
	return d->remainingTime(timerId);
}
#endif

#if defined(Q_OS_WIN) && QT_VERSION >= 0x050000
bool EventDispatcherLibUv::registerEventNotifier(QWinEventNotifier* notifier)
{
	Q_UNUSED(notifier)
	Q_UNIMPLEMENTED();
	return false;
}

void EventDispatcherLibUv::unregisterEventNotifier(QWinEventNotifier* notifier)
{
	Q_UNUSED(notifier)
	Q_UNIMPLEMENTED();
}
#endif

void EventDispatcherLibUv::wakeUp(void)
{
	Q_D(EventDispatcherLibUv);

#if QT_VERSION >= 0x040400
	if (d->m_wakeups.testAndSetAcquire(0, 1))
#endif
	{
		uv_async_send(&d->m_wakeup);
	}
}

void EventDispatcherLibUv::interrupt(void)
{
	Q_D(EventDispatcherLibUv);
	d->m_interrupt = true;
	this->wakeUp();
}

void EventDispatcherLibUv::flush(void)
{
}

EventDispatcherLibUv::EventDispatcherLibUv(EventDispatcherLibUvPrivate& dd, QObject* parent)
	: QAbstractEventDispatcher(parent), d_ptr(&dd)
{
}
