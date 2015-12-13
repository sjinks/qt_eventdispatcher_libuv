#include <QtCore/QCoreApplication>
#include <QtCore/QEvent>
#include <QtCore/QPair>
#include "eventdispatcher_libuv_p.h"

#ifdef WIN32
#	include "win32_utils.h"
#endif

namespace {

	static void calculateCoarseTimerTimeout(TimerInfo* info, const struct timeval& now, struct timeval& when)
	{
		Q_ASSERT(info->interval > 20);
		// The coarse timer works like this:
		//  - interval under 40 ms: round to even
		//  - between 40 and 99 ms: round to multiple of 4
		//  - otherwise: try to wake up at a multiple of 25 ms, with a maximum error of 5%
		//
		// We try to wake up at the following second-fraction, in order of preference:
		//    0 ms
		//  500 ms
		//  250 ms or 750 ms
		//  200, 400, 600, 800 ms
		//  other multiples of 100
		//  other multiples of 50
		//  other multiples of 25
		//
		// The objective is to make most timers wake up at the same time, thereby reducing CPU wakeups.

		int interval     = info->interval;
		int msec         = static_cast<int>(info->when.tv_usec / 1000);
		int max_rounding = interval / 20; // 5%
		when             = info->when;

		if (interval < 100 && (interval % 25) != 0) {
			if (interval < 50) {
				uint round_up = ((msec % 50) >= 25) ? 1 : 0;
				msec = ((msec >> 1) | round_up) << 1;
			}
			else {
				uint round_up = ((msec % 100) >= 50) ? 1 : 0;
				msec = ((msec >> 2) | round_up) << 2;
			}
		}
		else {
			int min = qMax<int>(0, msec - max_rounding);
			int max = qMin(1000, msec + max_rounding);

			bool done = false;

			// take any round-to-the-second timeout
			if (min == 0) {
				msec = 0;
				done = true;
			}
			else if (max == 1000) {
				msec = 1000;
				done = true;
			}

			if (!done) {
				int boundary;

				// if the interval is a multiple of 500 ms and > 5000 ms, always round towards a round-to-the-second
				// if the interval is a multiple of 500 ms, round towards the nearest multiple of 500 ms
				if ((interval % 500) == 0) {
					if (interval >= 5000) {
						msec = msec >= 500 ? max : min;
						done = true;
					}
					else {
						boundary = 500;
					}
				}
				else if ((interval % 50) == 0) {
					// same for multiples of 250, 200, 100, 50
					uint tmp = interval / 50;
					if ((tmp % 4) == 0) {
						boundary = 200;
					}
					else if ((tmp % 2) == 0) {
						boundary = 100;
					}
					else if ((tmp % 5) == 0) {
						boundary = 250;
					}
					else {
						boundary = 50;
					}
				}
				else {
					boundary = 25;
				}

				if (!done) {
					int base   = (msec / boundary) * boundary;
					int middle = base + boundary / 2;
					msec       = (msec < middle) ? qMax(base, min) : qMin(base + boundary, max);
				}
			}
		}

		if (msec == 1000) {
			++when.tv_sec;
			when.tv_usec = 0;
		}
		else {
			when.tv_usec = msec * 1000;
		}

		if (timercmp(&when, &now, <)) {
			when.tv_sec  += interval / 1000;
			when.tv_usec += (interval % 1000) * 1000;
			if (when.tv_usec > 999999) {
				++when.tv_sec;
				when.tv_usec -= 1000000;
			}
		}

		Q_ASSERT(timercmp(&now, &when, <=));
	}
}

uint64_t calculateNextTimeout(TimerInfo* info, const struct timeval& now)
{
	struct timeval tv_interval;
	struct timeval when;
	tv_interval.tv_sec  = info->interval / 1000;
	tv_interval.tv_usec = (info->interval % 1000) * 1000;

	if (info->interval) {
		qlonglong tnow  = (qlonglong(now.tv_sec)        * 1000) + (now.tv_usec        / 1000);
		qlonglong twhen = (qlonglong(info->when.tv_sec) * 1000) + (info->when.tv_usec / 1000);

		if ((info->interval < 1000 && twhen - tnow > 1500) || (info->interval >= 1000 && twhen - tnow > 1.2*info->interval)) {
			info->when = now;
		}
	}

	if (Qt::VeryCoarseTimer == info->type) {
		if (info->when.tv_usec >= 500000) {
			++info->when.tv_sec;
		}

		info->when.tv_usec = 0;
		info->when.tv_sec += info->interval / 1000;
		if (info->when.tv_sec <= now.tv_sec) {
			info->when.tv_sec = now.tv_sec + info->interval / 1000;
		}

		when = info->when;
	}
	else if (Qt::PreciseTimer == info->type) {
		if (info->interval) {
			timeradd(&info->when, &tv_interval, &info->when);
			if (timercmp(&info->when, &now, <)) {
				timeradd(&now, &tv_interval, &info->when);
			}

			when = info->when;
		}
		else {
			when = now;
		}
	}
	else {
		timeradd(&info->when, &tv_interval, &info->when);
		if (timercmp(&info->when, &now, <)) {
			timeradd(&now, &tv_interval, &info->when);
		}

		calculateCoarseTimerTimeout(info, now, when);
	}

	return static_cast<uint64_t>(when.tv_sec - now.tv_sec)*1000 + static_cast<uint64_t>(when.tv_usec - now.tv_usec)/1000;
}


void EventDispatcherLibUvPrivate::registerTimer(int timerId, int interval, Qt::TimerType type, QObject* object)
{
	Q_ASSERT(interval > 0);

	struct timeval now;
	gettimeofday(&now, 0);

	TimerInfo* info = new TimerInfo;
	info->timerId   = timerId;
	info->interval  = interval;
	info->type      = type;
	info->object    = object;
	info->when      = now; // calculateNextTimeout() will take care of info->when

	if (Qt::CoarseTimer == type) {
		if (interval >= 20000) {
			info->type = Qt::VeryCoarseTimer;
		}
		else if (interval <= 20) {
			info->type = Qt::PreciseTimer;
		}
	}

	uint64_t delta = calculateNextTimeout(info, now);

	uv_timer_t* tmp = &info->ev;
	uv_timer_init(this->m_base, tmp);
	info->ev.data = info;
	uv_timer_start(tmp, EventDispatcherLibUvPrivate::timer_callback, delta, 0);
	this->m_timers.insert(timerId, info);
}

void EventDispatcherLibUvPrivate::registerZeroTimer(int timerId, QObject* object)
{
	ZeroTimer data;
	data.object = object;
	data.active = true;
	this->m_zero_timers.insert(timerId, data);
}

bool EventDispatcherLibUvPrivate::unregisterTimer(int timerId)
{
	TimerHash::Iterator it = this->m_timers.find(timerId);
	if (it != this->m_timers.end()) {
		TimerInfo* info = it.value();
		uv_timer_stop(&info->ev);
		delete info;
		this->m_timers.erase(it);
		return true;
	}

	return this->m_zero_timers.remove(timerId) > 0;
}

bool EventDispatcherLibUvPrivate::unregisterTimers(QObject* object)
{
	bool result = false;
	TimerHash::Iterator it = this->m_timers.begin();
	while (it != this->m_timers.end()) {
		TimerInfo* info = it.value();

		if (object == info->object) {
			result = true;
			uv_timer_stop(&info->ev);
			delete info;
			it = this->m_timers.erase(it);
		}
		else {
			++it;
		}
	}

	ZeroTimerHash::Iterator zit = this->m_zero_timers.begin();
	while (zit != this->m_zero_timers.end()) {
		ZeroTimer& data = zit.value();
		if (object == data.object) {
			result = true;
			zit    = this->m_zero_timers.erase(zit);
		}
		else {
			++zit;
		}
	}

	return result;
}

QList<QAbstractEventDispatcher::TimerInfo> EventDispatcherLibUvPrivate::registeredTimers(QObject* object) const
{
	QList<QAbstractEventDispatcher::TimerInfo> res;

	TimerHash::ConstIterator it = this->m_timers.constBegin();
	while (it != this->m_timers.constEnd()) {
		TimerInfo* info = it.value();
		if (object == info->object) {
#if QT_VERSION < 0x050000
			QAbstractEventDispatcher::TimerInfo ti(it.key(), info->interval);
#else
			QAbstractEventDispatcher::TimerInfo ti(it.key(), info->interval, info->type);
#endif
			res.append(ti);
		}

		++it;
	}

	ZeroTimerHash::ConstIterator zit = this->m_zero_timers.constBegin();
	while (zit != this->m_zero_timers.constEnd()) {
		const ZeroTimer& data = zit.value();
		if (object == data.object) {
#if QT_VERSION < 0x050000
			QAbstractEventDispatcher::TimerInfo ti(it.key(), 0);
#else
			QAbstractEventDispatcher::TimerInfo ti(it.key(), 0, Qt::PreciseTimer);
#endif
			res.append(ti);
		}

		++zit;
	}

	return res;
}

int EventDispatcherLibUvPrivate::remainingTime(int timerId) const
{
	TimerHash::ConstIterator it = this->m_timers.find(timerId);
	if (it != this->m_timers.end()) {
		const TimerInfo* info = it.value();

		const uv_timer_t* tmp = &info->ev;
		if (uv_is_active(reinterpret_cast<const uv_handle_t*>(tmp))) {
			uint64_t now  = uv_now(this->m_base);
			uint64_t when = static_cast<uint64_t>(info->when.tv_sec)*1000 + static_cast<uint64_t>(info->when.tv_usec)/1000;

			if (now > when) {
				return 0;
			}

			return static_cast<int>((when - now) * 1000);
		}
	}

	// For zero timers we return -1 as well

	return -1;
}

void EventDispatcherLibUvPrivate::timer_callback(
	uv_timer_t* w
#if UV_VERSION_MAJOR < 1
	, int
#endif
)
{
	EventDispatcherLibUvPrivate* self = static_cast<EventDispatcherLibUvPrivate*>(w->loop->data);
	TimerInfo* info                   = static_cast<TimerInfo*>(w->data);

	// Timer can be reactivated only after its callback finishes; processEvents() will take care of this
	PendingEvent event(info->object, new QTimerEvent(info->timerId));
	self->m_event_list.append(event);
}

bool EventDispatcherLibUvPrivate::disableTimers(bool disable)
{
	struct timeval now;
	if (!disable) {
		gettimeofday(&now, 0);
	}

	TimerHash::Iterator it = this->m_timers.begin();
	while (it != this->m_timers.end()) {
		TimerInfo* info = it.value();
		if (disable) {
			uv_timer_stop(&info->ev);
		}
		else {
			uint64_t delta = calculateNextTimeout(info, now);
			uv_timer_t* tmp = &info->ev;
			uv_timer_start(tmp, &EventDispatcherLibUvPrivate::timer_callback, delta, 0);
		}

		++it;
	}

	return true;
}

void EventDispatcherLibUvPrivate::killTimers(void)
{
	if (!this->m_timers.isEmpty()) {
		TimerHash::Iterator it = this->m_timers.begin();
		while (it != this->m_timers.end()) {
			TimerInfo* info = it.value();
			uv_timer_stop(&info->ev);
			delete info;
			++it;
		}

		this->m_timers.clear();
	}
}
