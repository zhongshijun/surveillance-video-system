#-------------------------------------------------
#
# Project created by QtCreator 2016-08-17T16:23:50
#
#-------------------------------------------------
CONFIG +=c++14
QMAKE_CXXFLAGS +="-ftest-coverage -fprofile-arcs -msse -msse2 -msse3"
QMAKE_LFLAGS +="-lgcov --coverage"

QT       += widgets sql

TARGET = yzbxLib
TEMPLATE = lib

DEFINES += YZBXLIB_LIBRARY

SOURCES += qyzbxlib.cpp \
    yzbx_utility.cpp \
    qyzbxmaskfeatures.cpp \
    ../lib/bgsfactory_yzbx.cpp \
    ../lib/trackingfactory_yzbx.cpp \
    ../lib/urbantracker_tracking.cpp \
    ../extern/qt-json/json.cpp \
    lbp/lbp.cpp \
    lbp/histogram.cpp \
    ../lib/motionbasedtracker.cpp \
    frameinput.cpp \
    trackingStatus.cpp \
    trackingObjectAssociation.cpp \
    trackingkalman.cpp \
    HungarianAlg.cpp \
    Kalman.cpp \
    singleobjecttracker.cpp \
    trackingobjectfeature.cpp \
    multiobjecttracking.cpp

HEADERS += qyzbxlib.h\
        yzbxlib_global.h \
    yzbx_config.h \
    yzbx_utility.h \
    ../lib/bgsfactory_yzbx.h \
    ../lib/tracking_yzbx.h \
    ../lib/trackingfactory_yzbx.h \
    ../lib/urbantracker_tracking.h \
    ../extern/qt-json/json.h \
    lbp/lbp.hpp \
    lbp/histogram.hpp \
    ../lib/motionbasedtracker.h \
    frameinput.h \
    qyzbxTrackingFeatures.h \
    trackingStatus.h \
    trackingObjectAssociation.h \
    trackingkalman.h \
    HungarianAlg.h \
    Kalman.h \
    singleobjecttracker.h \
    trackingobjectfeature.h \
    multiobjecttracking.h

unix {
    target.path = /usr/lib
    INSTALLS += target
}