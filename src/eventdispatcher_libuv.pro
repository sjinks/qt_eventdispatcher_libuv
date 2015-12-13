QT      -= gui
TARGET   = eventdispatcher_libuv
TEMPLATE = lib
DESTDIR  = ../lib
CONFIG  += staticlib create_prl release
HEADERS += eventdispatcher_libuv.h eventdispatcher_libuv_p.h
SOURCES += eventdispatcher_libuv.cpp eventdispatcher_libuv_p.cpp timers_p.cpp socknot_p.cpp

headers.files = eventdispatcher_libuv.h

win32 {
	HEADERS += win32_utils.h
	SOURCES += win32_utils.cpp
}

unix {
	CONFIG += create_pc

	system('pkg-config --exists libuv') {
		CONFIG    += link_pkgconfig
		PKGCONFIG += libuv
	}
	else {
		LIBS += -luv -lrt -ldl
	}

	target.path   = /usr/lib
	headers.path  = /usr/include

	QMAKE_PKGCONFIG_NAME        = eventdispatcher_libuv
	QMAKE_PKGCONFIG_DESCRIPTION = "LibUv-based event dispatcher for Qt"
	QMAKE_PKGCONFIG_LIBDIR      = $$target.path
	QMAKE_PKGCONFIG_INCDIR      = $$headers.path
	QMAKE_PKGCONFIG_DESTDIR     = pkgconfig
}
else {
	LIBS        += -luv
	headers.path = $$DESTDIR
}

INSTALLS += target headers
