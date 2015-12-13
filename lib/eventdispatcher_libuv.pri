HEADERS += $$PWD/eventdispatcher_libuv.h

unix {
	CONFIG    += link_pkgconfig
	PKGCONFIG += eventdispatcher_libuv
}
else:win32 {
	LIBS        += -L$$PWD -leventdispatcher_libuv
	INCLUDEPATH += $$PWD
	DEPENDPATH  += $$PWD
}
