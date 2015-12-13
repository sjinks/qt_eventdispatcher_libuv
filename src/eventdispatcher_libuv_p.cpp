#include <QtCore/QCoreApplication>
#include "eventdispatcher_libuv.h"
#include "eventdispatcher_libuv_p.h"

#ifdef WIN32
#	include "win32_utils.h"
#endif

EventDispatcherLibUvPrivate::EventDispatcherLibUvPrivate(EventDispatcherLibUv* const q)
	: q_ptr(q), m_interrupt(false), m_base(0), m_wakeup(),
#if QT_VERSION >= 0x040400
	  m_wakeups(),
#endif
	  m_notifiers(), m_timers(), m_event_list(), m_zero_timers(), m_awaken(false)
{
#if UV_VERSION_MAJOR < 1
	this->m_base = uv_loop_new();
	if (!this->m_base) {
		qFatal("%s: failed to initialize the event loop", Q_FUNC_INFO);
	}
#else
	this->m_base = new uv_loop_t;
	uv_loop_init(this->m_base);
#endif

	this->m_base->data = this;

	uv_async_init(this->m_base, &this->m_wakeup, EventDispatcherLibUvPrivate::wake_up_handler);
}

EventDispatcherLibUvPrivate::~EventDispatcherLibUvPrivate(void)
{
	if (this->m_base) {
//		uv_close(&this->m_wakeup, 0);

		this->killTimers();
		this->killSocketNotifiers();

#if UV_VERSION_MAJOR < 1
		uv_loop_delete(this->m_base);
#else
		uv_loop_close(this->m_base);
		delete this->m_base;
#endif

		this->m_base = 0;
	}
}

bool EventDispatcherLibUvPrivate::processEvents(QEventLoop::ProcessEventsFlags flags)
{
	Q_Q(EventDispatcherLibUv);

	const bool exclude_notifiers = (flags & QEventLoop::ExcludeSocketNotifiers);
	const bool exclude_timers    = (flags & QEventLoop::X11ExcludeTimers);

	exclude_notifiers && this->disableSocketNotifiers(true);
	exclude_timers    && this->disableTimers(true);

	this->m_interrupt = false;
	this->m_awaken    = false;

	bool result = q->hasPendingEvents();

	Q_EMIT q->awake();
#if QT_VERSION < 0x040500
	QCoreApplication::sendPostedEvents(0, (flags & QEventLoop::DeferredDeletion) ? -1 : 0);
#else
	QCoreApplication::sendPostedEvents();
#endif

	bool can_wait = !this->m_interrupt && (flags & QEventLoop::WaitForMoreEvents) && !result;
	uv_run_mode f = UV_RUN_NOWAIT;

	if (!this->m_interrupt) {
		if (!exclude_timers && this->m_zero_timers.size() > 0) {
			result |= this->processZeroTimers();
			if (result) {
				can_wait = false;
			}
		}

		if (can_wait) {
			Q_EMIT q->aboutToBlock();
			f = UV_RUN_ONCE;
		}

		// Work around a bug when libev returns from ev_loop(loop, EVLOOP_ONESHOT) without processing any events
//		do {
			uv_run(this->m_base, f);
//		} while (can_wait && !this->m_awaken && !this->m_event_list.size());

#if QT_VERSION >= 0x040800
		EventList list;
		this->m_event_list.swap(list);
#else
		EventList list(this->m_event_list);
		this->m_event_list.clear();
#endif

		result |= (list.size() > 0) | this->m_awaken;

		for (int i=0; i<list.size(); ++i) {
			const PendingEvent& e = list.at(i);
			if (!e.first.isNull()) {
				QCoreApplication::sendEvent(e.first, e.second);
			}
		}

		struct timeval now;
		gettimeofday(&now, 0);

		// Now that all event handlers have finished (and we returned from the recusrion), reactivate all pending timers
		for (int i=0; i<list.size(); ++i) {
			const PendingEvent& e = list.at(i);
			if (!e.first.isNull() && e.second->type() == QEvent::Timer) {
				QTimerEvent* te = static_cast<QTimerEvent*>(e.second);
				TimerHash::Iterator tit = this->m_timers.find(te->timerId());
				if (tit != this->m_timers.end()) {
					TimerInfo* info = tit.value();

					uv_timer_t* tmp = &info->ev;
					if (!uv_is_active(reinterpret_cast<uv_handle_t*>(tmp))) { // false in tst_QTimer::restartedTimerFiresTooSoon()
						uint64_t delta = calculateNextTimeout(info, now);
						uv_timer_start(tmp, &EventDispatcherLibUvPrivate::timer_callback, delta, 0);
					}
				}
			}

			delete e.second;
		}
	}

	exclude_notifiers && this->disableSocketNotifiers(false);
	exclude_timers    && this->disableTimers(false);

	return result;
}

bool EventDispatcherLibUvPrivate::processZeroTimers(void)
{
	bool result    = false;
	QList<int> ids = this->m_zero_timers.keys();

	for (int i=0; i<ids.size(); ++i) {
		int tid = ids.at(i);
		ZeroTimerHash::Iterator it = this->m_zero_timers.find(tid);
		if (it != this->m_zero_timers.end()) {
			ZeroTimer& data = it.value();
			if (data.active) {
				data.active = false;

				QTimerEvent event(tid);
				QCoreApplication::sendEvent(data.object, &event);
				result   = true;

				it = this->m_zero_timers.find(tid);
				if (it != this->m_zero_timers.end()) {
					ZeroTimer& data = it.value();
					if (!data.active) {
						data.active = true;
					}
				}
			}
		}
	}

	return result;
}

void EventDispatcherLibUvPrivate::wake_up_handler(
	uv_async_t* w
#if UV_VERSION_MAJOR < 1
	, int
#endif
)
{
	EventDispatcherLibUvPrivate* disp = static_cast<EventDispatcherLibUvPrivate*>(w->loop->data);
	disp->m_awaken = true;

#if QT_VERSION >= 0x040400
	if (!disp->m_wakeups.testAndSetRelease(1, 0)) {
		qCritical("%s: internal error, wakeUps.testAndSetRelease(1, 0) failed!", Q_FUNC_INFO);
	}
#endif
}
