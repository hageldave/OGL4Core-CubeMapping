# QT project file, NOT intended for use with qmake
# just to be able to use the QTCreator IDE
TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
INCLUDEPATH += ../../datraw/
INCLUDEPATH += ../../gl3w/include/
INCLUDEPATH += ../../packages/
INCLUDEPATH += ../../OGL4CoreAPI/
INCLUDEPATH += ../../zlib/

HEADERS +=  CubeMapping.h
SOURCES +=  CubeMapping.cpp

