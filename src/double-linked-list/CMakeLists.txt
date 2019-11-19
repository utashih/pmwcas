set(UTIL_HEADERS
  doubly_linked_list.h
)

set(UTIL_SOURCES
  doubly_linked_list.cc
)

add_library(double-linked-list STATIC ${UTIL_SOURCES} ${UTIL_HEADERS})
target_link_libraries(double-linked-list glog::glog)

ADD_PMWCAS_TEST(doubly_linked_list_tests)
