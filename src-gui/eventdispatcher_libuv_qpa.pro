QT      += gui-private
CONFIG  += staticlib create_prl link_prl
TEMPLATE = lib
TARGET   = eventdispatcher_libuv_qpa
SOURCES  = eventdispatcher_libuv_qpa.cpp
HEADERS  = eventdispatcher_libuv_qpa.h
DESTDIR  = ../lib

LIBS           += -L$$PWD/../lib -leventdispatcher_libuv
INCLUDEPATH    += $$PWD/../src
DEPENDPATH     += $$PWD/../src
PRE_TARGETDEPS += $$PWD/../lib/libeventdispatcher_libuv.a
