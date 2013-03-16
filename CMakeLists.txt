cmake_minimum_required (VERSION 2.6)

project (MapServer)

include(CheckFunctionExists)
include(CheckIncludeFile)
include(CheckCSourceCompiles)



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

SET(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake ${PROJECT_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})



check_function_exists("strrstr"          HAVE_STRRSTR)
check_function_exists("strcasecmp"  HAVE_STRCASECMP)
check_function_exists("strcasestr"  HAVE_STRCASESTR)
check_function_exists("strdup"  HAVE_STRDUP)
check_function_exists("strlcat"  HAVE_STRLCAT)
check_function_exists("strlcpy"  HAVE_STRLCPY)
check_function_exists("strlen"  HAVE_STRLEN)
check_function_exists("strncasecmp"  HAVE_STRNCASECMP)
check_function_exists("vsnprintf"  HAVE_VSNPRINTF)
check_function_exists(lrintf HAVE_LRINTF)
check_function_exists(lrint HAVE_LRINT)

check_include_file(dlfcn.h HAVE_DLFCN_H)


check_c_source_compiles("
int main(int argc, char **argv) {
   long x=0,y=0;
   for(x=0;x<5;x++) {
     if(y>1) break;
     y=__sync_fetch_and_add(&x,1);
   }
}" HAVE_SYNC_FETCH_AND_ADD)

option(WITH_FLEX_BISON "Regenerate parser" OFF)
if(WITH_FLEX_BISON)
   find_package(BISON)
   find_package(FLEX)

   BISON_TARGET(MapfileParser mapparser.y ${CMAKE_CURRENT_BUILD_DIR}/mapparser.c)
   FLEX_TARGET(MapfileScanner maplexer.l ${CMAKE_CURRENT_BUILD_DIR}/maplexer.c COMPILE_FLAGS "--nounistd -Pmsyy -i")
   ADD_FLEX_BISON_DEPENDENCY(MapfileScanner MapfileParser)
endif(WITH_FLEX_BISON)

include_directories(${CMAKE_CURRENT_BINARY_DIR})

set(CMAKE_LINK_INTERFACE_LIBRARIES "")

add_library(mapserver SHARED
cgiutil.c mapgeos.c maporaclespatial.c mapsearch.c mapwms.c classobject.c
mapgml.c mapoutput.c mapwmslayer.c layerobject.c mapgraticule.c mapows.c
mapservutil.c mapxbase.c maphash.c mapowscommon.c mapshape.c mapxml.c mapbits.c
maphttp.c mapparser.c mapstring.c mapxmp.c mapcairo.c mapimageio.c
mappluginlayer.c mapsymbol.c mapchart.c mapimagemap.c mappool.c maptclutf.c
mapcluster.c mapio.c mappostgis.c maptemplate.c mapcontext.c mapjoin.c
mappostgresql.c mapthread.c mapcopy.c maplabel.c mapprimitive.c maptile.c
mapcpl.c maplayer.c mapproject.c maptime.c mapcrypto.c maplegend.c
mapprojhack.c maptree.c mapdebug.c maplexer.c mapquantization.c mapunion.c
mapdraw.c maplibxml2.c mapquery.c maputil.c strptime.c mapdrawgdal.c
mapraster.c mapuvraster.c mapdummyrenderer.c mapobject.c maprasterquery.c
mapwcs.c maperror.c mapogcfilter.c mapregex.c mapwcs11.c mapfile.c
mapogcfiltercommon.c maprendering.c mapwcs20.c mapgd.c mapogcsld.c
mapresample.c mapwfs.c mapgdal.c mapogcsos.c mapscale.c mapwfs11.c
mapgeomtransform.c mapogroutput.c mapsde.c mapwfslayer.c mapagg.cpp mapkml.cpp
mapgeomutil.cpp mapkmlrenderer.cpp
mapogr.cpp mapserver-api.c)

add_executable(mapserv mapserv.c)
target_link_libraries(mapserv mapserver)
add_executable(shp2img shp2img.c)
target_link_libraries(shp2img mapserver)

set(FREETYPE_DIR ${FREETYPE_DIR} /usr/X11)
find_package(Freetype REQUIRED)
include_directories(${FREETYPE_INCLUDE_DIRS})
target_link_libraries(mapserver ${FREETYPE_LIBRARY})

add_subdirectory("${PROJECT_SOURCE_DIR}/renderers/agg/")
target_link_libraries(mapserver agg)

option(WITH_PROJ "Choose if reprojection support should be built in" ON)

if(WITH_PROJ)
  find_package(Proj REQUIRED)
  include_directories(${PROJ_INCLUDE_DIR})
  target_link_libraries(mapserver ${PROJ_LIBRARY})
  set (USE_PROJ 1)
  message(STATUS "...Proj support activated")
endif (WITH_PROJ)

option(WITH_WMS "Enable WMS Server support (requires proj support)" ON)
if(WITH_WMS)
   if(USE_PROJ)
      set(USE_WMS_SVR 1)
      message(STATUS "...WMS server support enabled")
   else(USE_PROJ)
      message(ERROR "...WMS support disabled due to lack of proj support")
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
  if(GD_SUPPORTS_PNG)
     set(USE_GD_PNG 1)
  endif(GD_SUPPORTS_PNG)
  if(GD_SUPPORTS_GIF)
     set(USE_GD_GIF 1)
  endif(GD_SUPPORTS_GIF)
  if(GD_SUPPORTS_JPEG)
     set(USE_GD_JPEG 1)
  endif(GD_SUPPORTS_JPEG)
  if(GD_SUPPORTS_FREETYPE)
     set(USE_GD_FREETYPE 1)
  endif(GD_SUPPORTS_FREETYPE)

  message(STATUS "...GD support activated")
endif (WITH_GD)

option(WITH_FRIBIDI "Choose if FriBidi glyph shaping support should be built in (usefull for left-to-right languages)" ON)

if(WITH_FRIBIDI)
  find_package(FriBiDi REQUIRED)
  include_directories(${FRIBIDI_INCLUDE_DIR})
  target_link_libraries(mapserver ${FRIBIDI_LIBRARY})
  set (USE_FRIBIDI 1)
  message(STATUS "...FRIBIDI support activated")
  if(FRIBIDI_LEGACY)
     message(WARNING "Old Fribidi found, upgrade recommended")
  endif(FRIBIDI_LEGACY)
endif (WITH_FRIBIDI)

option(WITH_ICONV "Choose if Iconv Internationalization support should be built in" ON)

if(WITH_ICONV)
  find_package(ICONV REQUIRED)
  include_directories(${ICONV_INCLUDE_DIR})
  target_link_libraries(mapserver ${ICONV_LIBRARY})
  set (USE_ICONV 1)
  message(STATUS "...ICONV support activated")
endif (WITH_ICONV)


option(WITH_CAIRO "Choose if CAIRO  rendering support should be built in (required for SVG and PDF output)" ON)

if(WITH_CAIRO)
  find_package(Cairo REQUIRED)
  include_directories(${CAIRO_INCLUDE_DIRS})
  target_link_libraries(mapserver ${CAIRO_LIBRARIES})
  set (USE_CAIRO 1)
  message(STATUS "...CAIRO support activated")
endif (WITH_CAIRO)

option(WITH_MYSQL "Choose if MYSQL joining support should be built in" OFF)

if(WITH_MYSQL)
  find_package(MySQL REQUIRED)
  include_directories(${MYSQL_INCLUDE_DIR})
  target_link_libraries(mapserver ${MYSQL_LIBRARY})
  set (USE_MYSQL 1)
  message(STATUS "...MYSQL support activated")
endif (WITH_MYSQL)

option(WITH_FCGI "Choose if FastCGI support should be built in" ON)

if(WITH_FCGI)
  find_package(FCGI REQUIRED)
  include_directories(${FCGI_INCLUDE_DIR})
  target_link_libraries(mapserv ${FCGI_LIBRARY})
  set (USE_FASTCGI 1)
  message(STATUS "...FCGI support activated")
endif (WITH_FCGI)

option(WITH_GEOS "Choose if GEOS geometry operations support should be built in" ON)

if(WITH_GEOS)
   find_package(GEOS REQUIRED)
   include_directories(${GEOS_INCLUDE_DIR})
   target_link_libraries(mapserver ${GEOS_LIBRARY})
  set (USE_GEOS 1)
  message(STATUS "...GEOS support activated")
endif (WITH_GEOS)

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

option(WITH_CLIENT_WMS "Enable Client WMS Layer support (requires CURL and GDAL support)" ON)
option(WITH_CLIENT_WFS "Enable Client WMS Layer support (requires CURL and OGR support)" ON)

if((WITH_CLIENT_WMS) OR (WITH_CLIENT_WFS))
   find_package(CURL)
   include_directories(${CURL_INCLUDE_DIR})
   target_link_libraries(mapserver ${CURL_LIBRARY})
endif((WITH_CLIENT_WMS) OR (WITH_CLIENT_WFS))

if(WITH_CLIENT_WMS)
  if(!FOUND_GDAL)
     message(WARNING "Client WMS support requires GDAL support, disabling...")
     set(WITH_CLIENT_WMS 0)
  endif(!FOUND_GDAL)
  if(!CURL_FOUND)
     message(WARNING "Client WMS support requires CURL support, disabling...")
     set(WITH_CLIENT_WMS 0)
  endif(!CURL_FOUND)
endif(WITH_CLIENT_WMS)
if(WITH_CLIENT_WMS)
     message(STATUS "...Client WMS support activated")
     set(USE_WMS_LYR 1)
endif(WITH_CLIENT_WMS)

if(WITH_CLIENT_WFS)
  if(!USE_OGR)
     message(WARNING "Client WFS support requires OGR support, disabling...")
     set(WITH_CLIENT_WFS 0)
  endif(!USE_OGR)
  if(!CURL_FOUND)
     message(WARNING "Client WFS support requires CURL support, disabling...")
     set(WITH_CLIENT_WMS 0)
  endif(!CURL_FOUND)
endif(WITH_CLIENT_WFS)
if(WITH_CLIENT_WFS)
     message(STATUS "...Client WFS support activated")
     set(USE_WFS_LYR 1)
endif(WITH_CLIENT_WFS)

if((WITH_CLIENT_WMS) OR (WITH_CLIENT_WFS))
     message(STATUS "...CURL support required, added")
     set(USE_CURL 1)
endif((WITH_CLIENT_WMS) OR (WITH_CLIENT_WFS))

option(WITH_WFS "Enable WFS Server support (requires PROJ and OGR support)" ON)
if(WITH_WFS)
   if(USE_PROJ AND USE_OGR)
      set(USE_WFS_SVR 1)
      message(STATUS "...WFS server support enabled")
   else(USE_PROJ AND USE_OGR)
     if( NOT USE_OGR)
        message(ERROR "WFS Server support requires OGR support")
     endif( NOT USE_OGR)
     if(!USE_PROJ)
        message(WARNING "Client WFS support requires PROJ support")
     endif( NOT USE_PROJ)
   endif(USE_PROJ AND USE_OGR)
else(WITH_WFS)
   message(STATUS "...WFS server support disabled")
endif(WITH_WFS)

option(WITH_WCS "Enable WCS Server support (requires PROJ and GDAL support)" ON)
if(WITH_WCS)
   if(USE_PROJ AND USE_GDAL)
      set(USE_WCS_SVR 1)
      message(STATUS "...WCS server support enabled")
   else(USE_PROJ AND USE_GDAL)
     if(NOT USE_GDAL)
        message(ERROR "WCS Server support requires GDAL support")
     endif(NOT USE_GDAL)
     if(NOT USE_PROJ)
        message(WARNING "Client WCS support requires PROJ support")
     endif(NOT USE_PROJ)
   endif(USE_PROJ AND USE_GDAL)
else(WITH_WCS)
   message(STATUS "...WCS server support disabled")
endif(WITH_WCS)

option(WITH_LIBXML2 "Choose if libxml2 support should be built in (used for sos, wcs 1.1,2.0 and wfs 1.1)" ON)
if(WITH_LIBXML2)
  find_package(LibXml2 REQUIRED)
  include_directories(${LIBXML2_INCLUDE_DIR})
  target_link_libraries(mapserver ${LIBXML2_LIBRARIES})
  set (USE_LIBXML2 1)
  message(STATUS "...LIBXML2 support activated")
endif (WITH_LIBXML2)

if( USE_WCS_SVR AND NOT USE_LIBXML2 )
   message(WARNING "WCS 1.1 and 2.0 require libxml2 support but it was not found. WCS 1.1 and 2.0 will not be supported by this build")
endif( USE_WCS_SVR AND NOT USE_LIBXML2 )
if( USE_WFS_SVR AND NOT USE_LIBXML2 )
   message(WARNING "WFS 1.1 requires libxml2 support but it was not found. WFS 1.1 will not be supported by this build")
endif( USE_WFS_SVR AND NOT USE_LIBXML2 )

option(WITH_SOS "Enable SOS Server support (requires PROJ and libxml2 support)" ON)
if(WITH_SOS)
   if(USE_PROJ AND USE_LIBXML2)
      set(USE_SOS_SVR 1)
      message(STATUS "...SOS server support enabled")
   else(USE_PROJ AND USE_LIBXML2)
     if(NOT USE_LIBXML2)
        message(ERROR "SOS Server support requires LIBXML2 support")
     endif(NOT USE_LIBXML2)
     if(NOT USE_PROJ)
        message(WARNING "Client SOS support requires PROJ support")
     endif(NOT USE_PROJ)
   endif(USE_PROJ AND USE_LIBXML2)
else(WITH_SOS)
   message(STATUS "...SOS server support disabled")
endif(WITH_SOS)


option(WITH_THREADS "Choose if a thread-safe version of libmapserver should be built (only recommended for some mapscripts)" OFF)
if(WITH_THREADS)
   set( CMAKE_THREAD_PREFER_PTHREAD 1 )
   find_package(Threads REQUIRED)
   if (Threads_FOUND)
      target_link_libraries (mapserver ${CMAKE_THREAD_LIBS_INIT})
      set(USE_THREAD 1)
      message(STATUS "...thread safety support requested, added...")
   endif (Threads_FOUND)
endif(WITH_THREADS)


find_package(PNG REQUIRED)
include_directories(${PNG_INCLUDE_DIR})
target_link_libraries(mapserver ${PNG_LIBRARY})
set(USE_PNG 1)

find_package(JPEG REQUIRED)
include_directories(${JPEG_INCLUDE_DIR})
target_link_libraries(mapserver ${JPEG_LIBRARY})
set(USE_JPEG 1)

option(WITH_GIF "Enable GIF support (for PIXMAP loading)" ON)
if(WITH_GIF)
   find_package(GIF REQUIRED)
   include_directories(${GIF_INCLUDE_DIR})
   target_link_libraries(mapserver ${GIF_LIBRARY})
   if(GIF_FOUND)
      set(USE_GIF 1)
   endif(GIF_FOUND)
endif(WITH_GIF)

option(WITH_PYTHON "Enable Python mapscript support" ON)
if(WITH_PYTHON)
   add_subdirectory("mapscript/python")
endif(WITH_PYTHON)

option(WITH_PHP "Enable Python mapscript support" ON)
if(WITH_PHP)
   add_subdirectory("mapscript/php")
endif(WITH_PHP)

configure_file (
  "${PROJECT_SOURCE_DIR}/mapserver-config.h.in"
  "${PROJECT_BINARY_DIR}/mapserver-config.h"
  )

configure_file (
  "${PROJECT_SOURCE_DIR}/mapserver-version.h.in"
  "${PROJECT_BINARY_DIR}/mapserver-version.h"
  )


include_directories("${PROJECT_BINARY_DIR}")




INSTALL(FILES mapserver-api.h ${PROJECT_BINARY_DIR}/mapserver-version.h DESTINATION include)
INSTALL(TARGETS mapserver DESTINATION lib)
INSTALL(TARGETS shp2img mapserv DESTINATION bin)

