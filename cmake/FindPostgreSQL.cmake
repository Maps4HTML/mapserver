# - Find PostgreSQL
# Find the PostgreSQL includes and client library
# This module defines
#  POSTGRESQL_INCLUDE_DIR, where to find POSTGRESQL.h
#  POSTGRESQL_LIBRARY, the libraries needed to use POSTGRESQL.
#  POSTGRESQL_FOUND, If false, do not try to use PostgreSQL.
#
# Copyright (c) 2013 Thomas Bonfort
#

 FIND_PATH(POSTGRESQL_INCLUDE_DIR libpq-fe.h
      /usr/include/server
      /usr/include/postgresql
      /usr/include/pgsql/server
      /usr/local/include/pgsql/server
      /usr/include/postgresql/server
      /usr/include/postgresql/*/server
      /usr/local/include/postgresql/server
      /usr/local/include/postgresql/*/server
      $ENV{ProgramFiles}/PostgreSQL/*/include/server
      $ENV{SystemDrive}/PostgreSQL/*/include/server
      )

  find_library(POSTGRESQL_LIBRARY NAMES pq libpq
      PATHS
      /usr/lib
      /usr/local/lib
      /usr/lib/postgresql
      /usr/lib64
      /usr/local/lib64
      /usr/lib64/postgresql
      $ENV{ProgramFiles}/PostgreSQL/*/lib/ms
      $ENV{SystemDrive}/PostgreSQL/*/lib/ms
      )
      

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(POSTGRESQL DEFAULT_MSG POSTGRESQL_LIBRARY POSTGRESQL_INCLUDE_DIR)
MARK_AS_ADVANCED(POSTGRESQL_LIBRARY POSTGRESQL_INCLUDE_DIR)
