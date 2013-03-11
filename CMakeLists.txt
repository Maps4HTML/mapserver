cmake_minimum_required (VERSION 2.6)

project (MapServer)

include(CheckFunctionExists)

set (MapServer_VERSION_MAJOR 6)
set (MapServer_VERSION_MINOR 3)
set (MapServer_VERSION_REVISION 0)


MATH(EXPR MapServer_IS_DEV_VERSION "1-${MapServer_VERSION_MINOR}%2")
if(MapServer_IS_DEV_VERSION)
  set (MapServer_VERSION_STRING "${MapServer_VERSION_MAJOR}.${MapServer_VERSION_MINOR}.${MapServer_VERSION_REVISION}")
else(MapServer_IS_DEV_VERSION)
  set (MapServer_VERSION_STRING "${MapServer_VERSION_MAJOR}.${MapServer_VERSION_MINOR}-dev")
endif(MapServer_IS_DEV_VERSION)
MATH(EXPR MapServer_VERSION_NUM "${MapServer_VERSION_MAJOR}*10000+${MapServer_VERSION_MINOR}*100+${MapServer_VERSION_REVISION}")

SET(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})



check_function_exists("strrstr"          HAVE_STRRSTR)


add_library(mapserver SHARED cgiutil.c             mapgeos.c             maporaclespatial.c    mapsearch.c           mapwms.c
classobject.c         mapgml.c              mapoutput.c           mapwmslayer.c
layerobject.c         mapgraticule.c        mapows.c              mapservutil.c         mapxbase.c
maphash.c             mapowscommon.c        mapshape.c            mapxml.c
mapbits.c             maphttp.c             mapparser.c           mapstring.c           mapxmp.c
mapcairo.c            mapimageio.c          mappluginlayer.c      mapsymbol.c           
mapchart.c            mapimagemap.c         mappool.c             maptclutf.c          
mapcluster.c          mapio.c               mappostgis.c          maptemplate.c         
mapcontext.c          mapjoin.c             mappostgresql.c       mapthread.c           
mapcopy.c             maplabel.c            mapprimitive.c        maptile.c             
mapcpl.c              maplayer.c            mapproject.c          maptime.c             
mapcrypto.c           maplegend.c           mapprojhack.c         maptree.c             
mapdebug.c            maplexer.c            mapquantization.c     mapunion.c            
mapdraw.c             maplibxml2.c          mapquery.c            maputil.c             strptime.c
mapdrawgdal.c         mapraster.c           mapuvraster.c         
mapdummyrenderer.c    mapobject.c           maprasterquery.c      mapwcs.c              
maperror.c            mapogcfilter.c        mapregex.c            mapwcs11.c           
mapfile.c             mapogcfiltercommon.c  maprendering.c        mapwcs20.c         
mapgd.c               mapogcsld.c           mapresample.c         mapwfs.c
mapgdal.c             mapogcsos.c           mapscale.c            mapwfs11.c
mapgeomtransform.c    mapogroutput.c        mapsde.c              mapwfslayer.c
mapagg.cpp          mapkml.cpp          mapogl.cpp          mapoglrenderer.cpp
mapgeomutil.cpp     mapkmlrenderer.cpp  mapoglcontext.cpp   mapogr.cpp)

find_package(FREETYPE REQUIRED)
include_directories(${FREETYPE_INCLUDE_DIRS})
target_link_libraries(mapserver ${FREETYPE_LIBRARY})

add_subdirectory("${PROJECT_SOURCE_DIR}/renderers/agg/")
target_link_libraries(mapserver agg)

option(WITH_PROJ "Choose if reprojection support should be built in" ON)

if(WITH_PROJ)
  find_package(PROJ REQUIRED)
  include_directories(${PROJ_INCLUDE_DIR})
  target_link_libraries(mapserver ${PROJ_LIBRARY})
  set (USE_PROJ 1)
  message(STATUS "...Proj support activated")
endif (WITH_PROJ)

option(WITH_WMS "EnableWMS Server support (requires proj support)" ON)

if(WITH_WMS)
   if(USE_PROJ)
      set(USE_WMS_SVR 1)
   else(USE_PROJ)
      message(STATUS "...WMS support disabled due to lack of proj support")
   endif(USE_PROJ)
else(WITH_WMS)
   message(STATUS "...WMS server support disabled")
endif(WITH_WMS)

option(WITH_GD "Choose if (old) GD support should be built in" OFF)

if(WITH_GD)
  find_package(GD REQUIRED)
  include_directories(${GD_INCLUDE_DIR})
  target_link_libraries(mapserver ${GD_LIBRARY})
  set (USE_GD 1)
  message(STATUS "...GD support activated")
endif (WITH_GD)

option(WITH_POSTGIS "Choose if Postgis input support should be built in" ON)

if(WITH_POSTGIS)
   find_package(PostgreSQL REQUIRED)
   include_directories(${PostgreSQL_INCLUDE_DIR})
   target_link_libraries(mapserver ${PostgreSQL_LIBRARY})
  set (USE_POSTGIS 1)
  message(STATUS "...POSTGIS support activated")
endif (WITH_POSTGIS)

option(WITH_GDAL "Choose if GDAL input raster support should be built in" ON)
if(WITH_GDAL)
  find_package(GDAL REQUIRED)
  include_directories(${GDAL_INCLUDE_DIR})
  target_link_libraries(mapserver ${GDAL_LIBRARY})
  set (USE_GDAL 1)
  message(STATUS "...GDAL support activated")
endif (WITH_GDAL)

option(WITH_OGR "Choose if OGR/GDAL input vector support should be built in" ON)
if(WITH_OGR)
  if(FOUND_GDAL)
    set (USE_OGR 1)
    message(STATUS "...OGR support activated")
  else(FOUND_GDAL)
    find_package(GDAL REQUIRED)
    include_directories(${GDAL_INCLUDE_DIR})
    target_link_libraries(mapserver ${GDAL_LIBRARY})
    set (USE_OGR 1)
    message(STATUS "...OGR support activated")
  endif(FOUND_GDAL)
endif(WITH_OGR)



find_package(PNG REQUIRED)
include_directories(${PNG_INCLUDE_DIR})
target_link_libraries(mapserver ${PNG_LIBRARY})

find_package(JPEG REQUIRED)
include_directories(${JPEG_INCLUDE_DIR})
target_link_libraries(mapserver ${JPEG_LIBRARY})

configure_file (
  "${PROJECT_SOURCE_DIR}/mapserver-config.h.in"
  "${PROJECT_BINARY_DIR}/mapserver-config.h"
  )


include_directories("${PROJECT_BINARY_DIR}")

add_executable(mapserv mapserv.c)
target_link_libraries(mapserv mapserver)
add_executable(shp2img shp2img.c)
target_link_libraries(shp2img mapserver)




