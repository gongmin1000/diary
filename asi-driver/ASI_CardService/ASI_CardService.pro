TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt


LIBS += -lboost_system-mt -lboost_thread-mt -lpthread

INCLUDEPATH +=./

SOURCES += main.cpp \
    config.cpp \
    tinyxml2.cpp \
    asiport.cpp \
    asidev.cpp \
    cycbuf.cpp

HEADERS += \
    config.h \
    tinyxml2.h \
    asiport.h \
    asidev.h \
    cycbuf.h

