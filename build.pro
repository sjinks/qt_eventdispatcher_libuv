TEMPLATE = subdirs
CONFIG  += ordered

SUBDIRS = src tests

src.file   = src/eventdispatcher_libuv.pro
tests.file = tests/qt_eventdispatcher_tests/build.pro
