FIND_PACKAGE(SWIG REQUIRED)
INCLUDE(${SWIG_USE_FILE})
FIND_PACKAGE(Perl REQUIRED)
FIND_PACKAGE(PerlLibs REQUIRED)
INCLUDE_DIRECTORIES(${PERL_INCLUDE_PATH})
include_directories(${PROJECT_SOURCE_DIR}/mapscript/swiginc)
include_directories(${PROJECT_SOURCE_DIR}/mapscript/)
add_definitions(${PERL_EXTRA_C_FLAGS})
include_directories(${PROJECT_SOURCE_DIR}/mapscript/perl)
SWIG_ADD_MODULE(perlmapscript perl5 ../mapscript.i)

SWIG_LINK_LIBRARIES(perlmapscript ${PERL_LIBRARIES} mapserver)

#get_target_property(LOC_MAPSCRIPT_LIB ${SWIG_MODULE_mapscript_REAL_NAME} LOCATION)
#set(mapscript_files ${LOC_MAPSCRIPT_LIB} ${CMAKE_CURRENT_BINARY_DIR}/mapscript.py)
#install(FILES ${mapscript_files} DESTINATION ${PYTHON_SITE_PACKAGES})

#install(FILES ${CMAKE_CURRENT_BINARY_DIR}/mapscript.py DESTINATION ${PYTHON_SITE_PACKAGES})
#install(TARGETS mapscript DESTINATION ${PYTHON_SITE_PACKAGES})
