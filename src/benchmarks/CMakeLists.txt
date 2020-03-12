set(BENCHMARK_HEADERS
  ../util/random_number_generator.h
)

ADD_PMWCAS_BENCHMARK(mwcas_benchmark)
ADD_PMWCAS_BENCHMARK(mwcas_shm_server)

get_directory_property(COMPDEFS COMPILE_DEFINITIONS)
if(COMPDEFS MATCHES "^PMEM" OR COMPDEFS MATCHES ";PMEM")
  ADD_PMWCAS_BENCHMARK(doubly_linked_list_benchmark)
endif()
