cmake_minimum_required (VERSION 2.8.5)
project(gmailnotifier)
add_executable(gmailnotifier gmailnotifier.c gmailxml.c gmailxml.h)
add_executable(gmail_new_mails gmail_new_mails.c)

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif (NOT CMAKE_BUILD_TYPE)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/gpgme")

# Find gpg
find_package(Gpgme REQUIRED)
include_directories(${GPGME_INCLUDES})
set(GMAILNOTIFIERLIBS ${GMAILNOTIFIERLIBS} ${GPGME_VANILLA_LIBRARIES})

# Find libxml
find_package(LibXml2 REQUIRED)
include_directories(${LIBXML2_INCLUDE_DIR})
set(GMAILNOTIFIERLIBS ${GMAILNOTIFIERLIBS} ${LIBXML2_LIBRARIES})
set(GMAILNOTIFIERCFLAGS ${GMAILNOTIFIERCFLAGS} ${LIBXML2_DEFINITIONS})

# Find libnotify
find_package(Libnotify REQUIRED)
include_directories(${LIBNOTIFY_INCLUDE_DIR})
set(GMAILNOTIFIERLIBS ${GMAILNOTIFIERLIBS} ${LIBNOTIFY_LIBRARIES})

# Find rpass
set(GMAILNOTIFIERLIBS ${GMAILNOTIFIERLIBS} rpass)

# Find curl
find_package(CURL REQUIRED)
include_directories(${CURL_INCLUDE_DIRS})
set(GMAILNOTIFIERLIBS ${GMAILNOTIFIERLIBS} ${CURL_LIBRARIES})

set_target_properties(gmailnotifier PROPERTIES COMPILE_FLAGS "${GMAILNOTIFIERCFLAGS}")
target_link_libraries(gmailnotifier ${GMAILNOTIFIERLIBS})

install (TARGETS gmailnotifier gmail_new_mails DESTINATION bin)
