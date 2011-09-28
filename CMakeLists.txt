cmake_minimum_required (VERSION 2.8.5)
project(gmailnotifier)
add_executable(gmailnotifier gmailnotifier.c gmailxml.c gmailxml.h)
add_executable(gmail_new_mails gmail_new_mails.c)

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif (NOT CMAKE_BUILD_TYPE)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

# Find libxml
find_package(LibXml2 REQUIRED)
include_directories(${LIBXML2_INCLUDE_DIR})
set(GMAILNOTIFIERLIBS ${GMAILNOTIFIERLIBS} ${LIBXML2_LIBRARIES})
set(GMAILNOTIFIERCFLAGS ${GMAILNOTIFIERCFLAGS} ${LIBXML2_DEFINITIONS})

# Find libnotify
find_package(Libnotify REQUIRED)
include_directories(${LIBNOTIFY_INCLUDE_DIR})
set(GMAILNOTIFIERLIBS ${GMAILNOTIFIERLIBS} ${LIBNOTIFY_LIBRARIES})

# Find curl
find_package(CURL REQUIRED)
include_directories(${CURL_INCLUDE_DIRS})
set(GMAILNOTIFIERLIBS ${GMAILNOTIFIERLIBS} ${CURL_LIBRARIES})

# Find rpass
find_package(Librpass)
include_directories(${LIBRPASS_INCLUDE_DIR})
set(GMAILNOTIFIERLIBS ${GMAILNOTIFIERLIBS} ${LIBRPASS_LIBRARIES})

set_target_properties(gmailnotifier PROPERTIES COMPILE_FLAGS "${GMAILNOTIFIERCFLAGS}")
target_link_libraries(gmailnotifier ${GMAILNOTIFIERLIBS})

install (TARGETS gmailnotifier gmail_new_mails DESTINATION bin)
