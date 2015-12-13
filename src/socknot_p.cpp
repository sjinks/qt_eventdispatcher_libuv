#include <QtCore/QCoreApplication>
#include <QtCore/QEvent>
#include <QtCore/QSocketNotifier>
#include "eventdispatcher_libuv_p.h"

void EventDispatcherLibUvPrivate::registerSocketNotifier(QSocketNotifier* notifier)
{
	int sockfd = notifier->socket();
	int what;
	switch (notifier->type()) {
		case QSocketNotifier::Read:  what = UV_READABLE; break;
		case QSocketNotifier::Write: what = UV_WRITABLE; break;
		case QSocketNotifier::Exception: /// FIXME
			return;

		default:
			Q_ASSERT(false);
			return;
	}

	uv_poll_t* data = new uv_poll_t;
	uv_poll_init(this->m_base, data, sockfd);
	data->data = notifier;
	uv_poll_start(data, what, &EventDispatcherLibUvPrivate::socket_notifier_callback);

	this->m_notifiers.insert(notifier, data);
}

void EventDispatcherLibUvPrivate::unregisterSocketNotifier(QSocketNotifier* notifier)
{
	SocketNotifierHash::Iterator it = this->m_notifiers.find(notifier);
	if (it != this->m_notifiers.end()) {
		uv_poll_t* data = it.value();
		Q_ASSERT(data->data == notifier);

		uv_poll_stop(data);
		delete data;
		it = this->m_notifiers.erase(it);
	}
}

void EventDispatcherLibUvPrivate::socket_notifier_callback(uv_poll_t* w, int status, int events)
{
	Q_UNUSED(status)

	EventDispatcherLibUvPrivate* disp = static_cast<EventDispatcherLibUvPrivate*>(w->loop->data);
	QSocketNotifier* notifier         = static_cast<QSocketNotifier*>(w->data);
	QSocketNotifier::Type type        = notifier->type();

	if ((QSocketNotifier::Read == type && (events & UV_READABLE)) || (QSocketNotifier::Write == type && (events & UV_WRITABLE))) {
		PendingEvent event(notifier, new QEvent(QEvent::SockAct));
		disp->m_event_list.append(event);
	}
}

bool EventDispatcherLibUvPrivate::disableSocketNotifiers(bool disable)
{
	SocketNotifierHash::Iterator it = this->m_notifiers.begin();
	while (it != this->m_notifiers.end()) {
		uv_poll_t* data = it.value();
		if (disable) {
			uv_poll_stop(data);
		}
		else {
			QSocketNotifier* notifier = it.key();
			int what;
			switch (notifier->type()) {
				case QSocketNotifier::Read:  what = UV_READABLE; break;
				case QSocketNotifier::Write: what = UV_WRITABLE; break;
				case QSocketNotifier::Exception: /// FIXME
					continue;

				default:
					Q_ASSERT(false);
					continue;
			}

			uv_poll_start(data, what, &EventDispatcherLibUvPrivate::socket_notifier_callback);
		}

		++it;
	}

	return true;
}

void EventDispatcherLibUvPrivate::killSocketNotifiers(void)
{
	if (!this->m_notifiers.isEmpty()) {
		SocketNotifierHash::Iterator it = this->m_notifiers.begin();
		while (it != this->m_notifiers.end()) {
			uv_poll_t* data = it.value();
			uv_poll_stop(data);
			delete data;
			++it;
		}

		this->m_notifiers.clear();
	}
}
