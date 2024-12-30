QT       += core gui network widgets

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

SOURCES += \
    main.cc \
    mainwindow.cpp

HEADERS += \
    mainwindow.h \
    SpotifyClient.h

LIBS += -lcurl

INCLUDEPATH += /opt/homebrew/Cellar/nlohmann-json/3.11.3/include 

RESOURCES += \
    resources.qrc 