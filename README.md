# qt_eventdispatcher_libuv [![Build Status](https://secure.travis-ci.org/sjinks/qt_eventdispatcher_libuv.png)](http://travis-ci.org/sjinks/qt_eventdispatcher_libuv)

libuv-based event dispatcher for Qt


## Features
* very fast :-)
* compatible with Qt 4 and Qt 5
* does not use any private Qt headers
* passes Qt 4 and Qt 5 event dispatcher, event loop, timer and socket notifier tests


## Unsupported Features
* `QSocketNotifier::Exception` (libuv offers no support for this)
* Qt 5/Windows only: `QWinEventNotifier` is not supported (`registerEventNotifier()` and `unregisterEventNotifier()` functions are currently implemented as stubs)


## Requirements
* libuv >= 0.10
* Qt >= 4.2.1 (tests from tests-qt4 were run only on Qt 4.8.x, 4.5.4, 4.3.0, 4.2.1)


## Build

```
cd src
qmake
make
```

Replace `make` with `nmake` if your are using Microsoft Visual C++.

The above commands will generate the static library and `.prl` file in `../lib` directory.


## Install

After completing Build step run

*NIX:
```
sudo make install
```

Windows:
```
nmake install
```

For Windows this will copy `eventdispatcher_libuv.h` to `../lib` directory.
For *NIX this will install eventdispatcher_libuv.h to `/usr/include`, `libeventdispatcher_libuv.a` and `libeventdispatcher_libuv.prl` to `/usr/lib`, `eventdispatcher_libuv.pc` to `/usr/lib/pkgconfig`.


## Usage (Qt 4)

Simply include the header file and instantiate the dispatcher in `main()`
before creating the Qt application object.

```c++
#include "eventdispatcher_libuv.h"

int main(int argc, char** argv)
{
    EventDispatcherLibUv dispatcher;
    QCoreApplication app(argc, argv);

    // ...

    return app.exec();
}
```

And add these lines to the .pro file:

```
unix {
    CONFIG    += link_pkgconfig
    PKGCONFIG += eventdispatcher_libuv
}
else:win32 {
    include(/path/to/qt_eventdispatcher_libuv/lib/eventdispatcher_libuv.pri)
}
```

or

```
HEADERS += /path/to/eventdispatcher_libuv.h
LIBS    += -L/path/to/library -leventdispatcher_libuv
```


## Usage (Qt 5)

Simply include the header file and instantiate the dispatcher in `main()`
before creating the Qt application object.

```c++
#include "eventdispatcher_libuv.h"

int main(int argc, char** argv)
{
    QCoreApplication::setEventDispatcher(new EventDispatcherLibUv);
    QCoreApplication app(argc, argv);

    // ...

    return app.exec();
}
```

And add these lines to the .pro file:

```
unix {
    CONFIG    += link_pkgconfig
    PKGCONFIG += eventdispatcher_libuv
}
else:win32 {
    include(/path/to/qt_eventdispatcher_libev/lib/eventdispatcher_libuv.pri)
}
```

or

```
HEADERS += /path/to/eventdispatcher_libuv.h
LIBS    += -L/path/to/library -leventdispatcher_libuv
```

Qt 5 allows to specify a custom event dispatcher for the thread:

```c++
QThread* thr = new QThread;
thr->setEventDispatcher(new EventDispatcherLibUv);
```
