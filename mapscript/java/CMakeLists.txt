FIND_PACKAGE(SWIG REQUIRED)
INCLUDE(${SWIG_USE_FILE})
FIND_PACKAGE(JNI REQUIRED)
FIND_PACKAGE(Java REQUIRED)
INCLUDE_DIRECTORIES(${JAVA_INCLUDE_PATH} ${JAVA_INCLUDE_PATH2})
include_directories(${PROJECT_SOURCE_DIR}/mapscript/swiginc)
include_directories(${PROJECT_SOURCE_DIR}/mapscript/)
include_directories(${PROJECT_SOURCE_DIR}/mapscript/java)
SET (CMAKE_SWIG_OUTDIR "${CMAKE_CURRENT_BINARY_DIR}/edu/umn/gis/mapscript")
SET(CMAKE_SWIG_FLAGS -package edu.umn.gis.mapscript)
SWIG_ADD_MODULE(javamapscript java ../mapscript.i)

SWIG_LINK_LIBRARIES(javamapscript ${JAVA_LIBRARIES} mapserver)

ADD_CUSTOM_COMMAND(TARGET javamapscript
                      POST_BUILD
                      COMMAND cmake -E echo "Compiling Java files..."
                      COMMAND ${Java_JAVAC_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/edu/umn/gis/mapscript/*.java
                      COMMAND cmake -E echo "Creating jar file..."
                      COMMAND ${Java_JAR_EXECUTABLE} cvf mapscript.jar edu
                      )

#execute_process ( COMMAND ${PYTHON_EXECUTABLE} -c "from distutils.sysconfig import get_python_lib; print get_python_lib(True)" OUTPUT_VARIABLE PYTHON_SITE_PACKAGES OUTPUT_STRIP_TRAILING_WHITESPACE)
#
#get_target_property(LOC_MAPSCRIPT_LIB ${SWIG_MODULE_mapscript_REAL_NAME} LOCATION)
#set(mapscript_files ${LOC_MAPSCRIPT_LIB} ${CMAKE_CURRENT_BINARY_DIR}/mapscript.py)
#install(FILES ${mapscript_files} DESTINATION ${PYTHON_SITE_PACKAGES})

#install(FILES ${CMAKE_CURRENT_BINARY_DIR}/mapscript.py DESTINATION ${PYTHON_SITE_PACKAGES})
#install(TARGETS mapscript DESTINATION ${PYTHON_SITE_PACKAGES})
