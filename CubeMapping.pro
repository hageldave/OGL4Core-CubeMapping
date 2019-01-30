# QT project file, NOT intended for use with qmake
# just to be able to use the QTCreator IDE
TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
INCLUDEPATH += ../../
INCLUDEPATH += ../../gl3w/include/
INCLUDEPATH += ../../datraw/
INCLUDEPATH += ../../OGL4CoreAPI/
INCLUDEPATH += ../../zlib/

HEADERS +=  CubeMapping.h
SOURCES +=  CubeMapping.cpp \
            resources/box.frag.glsl \
            resources/box.vert.glsl \
            resources/cube.frag.glsl \
            resources/cube.geom.glsl \
            resources/cube.vert.glsl \
            resources/layers2cubemap.frag.glsl \
            resources/layers2cubemap.geom.glsl \
            resources/layers2cubemap.vert.glsl \
            resources/mirrorcube.frag.glsl \
            resources/mirrorcube.geom.glsl \
            resources/quad.frag.glsl \
            resources/quad.vert.glsl \
            resources/skybox.frag.glsl \
            resources/skybox.geom.glsl \
            resources/skybox.vert.glsl


