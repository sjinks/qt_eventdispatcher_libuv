#ifndef EVENTDISPATCHER_LIBUV_P_H
#define EVENTDISPATCHER_LIBUV_P_H

#include <qplatformdefs.h>
#include <QtCore/QAbstractEventDispatcher>
#include <QtCore/QHash>
#include <QtCore/QPointer>
#include <uv.h>

#if QT_VERSION >= 0x040400
#	include <QtCore/QAtomicInt>
#endif

#include "qt4compat.h"

struct TimerInfo {
	QObject* object;
	uv_timer_t ev;
	struct timeval when;
	int timerId;
	int interval;
	Qt::TimerType type;
};

struct ZeroTimer {
	QObject* object;
	bool active;
};

Q_DECLARE_TYPEINFO(TimerInfo, Q_PRIMITIVE_TYPE);
Q_DECLARE_TYPEINFO(ZeroTimer, Q_PRIMITIVE_TYPE);

Q_DECL_HIDDEN uint64_t calculateNextTimeout(TimerInfo* info, const struct timeval& now);

class EventDispatcherLibUv;

class Q_DECL_HIDDEN EventDispatcherLibUvPrivate {
public:
	EventDispatcherLibUvPrivate(EventDispatcherLibUv* const q);
	~EventDispatcherLibUvPrivate(void);
	bool processEvents(QEventLoop::ProcessEventsFlags flags);
	bool processZeroTimers(void);
	void registerSocketNotifier(QSocketNotifier* notifier);
	void unregisterSocketNotifier(QSocketNotifier* notifier);
	void registerTimer(int timerId, int interval, Qt::TimerType type, QObject* object);
	void registerZeroTimer(int timerId, QObject* object);
	bool unregisterTimer(int timerId);
	bool unregisterTimers(QObject* object);
	QList<QAbstractEventDispatcher::TimerInfo> registeredTimers(QObject* object) const;
	int remainingTime(int timerId) const;

	typedef QHash<QSocketNotifier*, uv_poll_t*> SocketNotifierHash;
	typedef QHash<int, TimerInfo*> TimerHash;
	typedef QPair<QPointer<QObject>, QEvent*> PendingEvent;
	typedef QList<PendingEvent> EventList;
	typedef QHash<int, ZeroTimer> ZeroTimerHash;

private:
	Q_DISABLE_COPY(EventDispatcherLibUvPrivate)
	Q_DECLARE_PUBLIC(EventDispatcherLibUv)
	EventDispatcherLibUv* const q_ptr;

	bool m_interrupt;
	uv_loop_t* m_base;
	uv_async_t m_wakeup;
#if QT_VERSION >= 0x040400
	QAtomicInt m_wakeups;
#endif
	SocketNotifierHash m_notifiers;
	TimerHash m_timers;
	EventList m_event_list;
	ZeroTimerHash m_zero_timers;
	bool m_awaken;

	static void socket_notifier_callback(uv_poll_t* w, int status, int events);
	static void timer_callback(
		uv_timer_t* w
#if UV_VERSION_MAJOR < 1
		, int status
#endif
	);
	static void wake_up_handler(
		uv_async_t* w
#if UV_VERSION_MAJOR < 1
		, int status
#endif
	);

	bool disableSocketNotifiers(bool disable);
	void killSocketNotifiers(void);
	bool disableTimers(bool disable);
	void killTimers(void);
};

#endif // EVENTDISPATCHER_LIBUV_P_H
