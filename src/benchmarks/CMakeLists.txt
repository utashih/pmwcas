set(BENCHMARK_HEADERS
  ../util/random_number_generator.h
)

if(${PMEM_BACKEND} STREQUAL "PMDK")
  ADD_PMWCAS_BENCHMARK(doubly_linked_list_benchmark)
endif()
