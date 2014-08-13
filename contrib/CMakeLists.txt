file(GLOB_RECURSE EPEE epee/include/*)

source_group(epee FILES ${EPEE})

add_library(epee ${EPEE})

set_property(TARGET epee PROPERTY FOLDER "external")
