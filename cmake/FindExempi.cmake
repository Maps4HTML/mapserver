# Look for the header file.
find_path(LIBEXEMPI_INCLUDE_DIR NAMES xmp.h
    PATH_SUFFIXES exempi-2.0 exempi-2.0/exempi exempi 
)

# Look for the library.
find_library(LIBEXEMPI_LIBRARY NAMES exempi)

# Handle the QUIETLY and REQUIRED arguments and set FCGI_FOUND to TRUE if all listed variables are TRUE.
include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(LIBEXEMPI DEFAULT_MSG LIBEXEMPI_LIBRARY LIBEXEMPI_INCLUDE_DIR)

# Copy the results to the output variables.
if(LIBEXEMPI_FOUND)
	set(LIBEXEMPI_LIBRARIES ${LIBEXEMPI_LIBRARY})
	set(LIBEXEMPI_INCLUDE_DIRS ${LIBEXEMPI_INCLUDE_DIR})
else()
	set(LIBEXEMPI_LIBRARIES)
	set(LIBEXEMPI_INCLUDE_DIRS)
endif()

mark_as_advanced(LIBEXEMPI_INCLUDE_DIR LIBEXEMPI_LIBRARY)
