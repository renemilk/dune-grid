# Module that checks whether Grape is available
#
# Accepts the following input variable
# GRAPE_PREFIX: Prefix under which Grape is installed
# GRAPE_INCLUDE_DIRS: Include directories for Grape
# GRAPE_LIBRARY: Full path to Grape library
#
# The following variable will be set:
# GRAPE_FOUND: whether Grape is available
# GRAPE_INCLUDE_DIRS: Include directories for Grape
# GRAPE_LIBRARIES: Full path to libraries needed to link
#   to Grape
#
# Provides the function
# add_dune_grape_flags( [OBJECT | SOURCE_ONLY] target1 ...)
#   that sets all necessary flags needed for compilation and linking.
#
set(GRAPE_FOUND GRAPE_FOUND-NOTFOUND)
