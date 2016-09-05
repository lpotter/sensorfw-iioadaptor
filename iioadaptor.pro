QT += dbus

CONFIG += qt debug warn_on link_prl link_pkgconfig plugin

PKGCONFIG += sensord-qt5
for(PKG, $$list($$unique(PKGCONFIG))) {
     !system(pkg-config --exists $$PKG):error($$PKG development files are missing)
}

CONFIG += link_pkgconfig
PKGCONFIG += udev
LIBS += -ludev

#PKGCONFIG += libiio
#LIBS += -liio

TEMPLATE = lib

TARGET       = iioaccelerometeradaptor-qt5

HEADERS += iioadaptor.h \
           iioadaptorplugin.h

SOURCES += iioadaptor.cpp \
           iioadaptorplugin.cpp

target.path = /usr/lib/sensord-qt5
INSTALLS += target
