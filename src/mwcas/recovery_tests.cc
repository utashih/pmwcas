// Copyright (c) Xiangpeng Hao (haoxiangpeng@hotmail.com). All rights reserved.
// Licensed under the MIT license.

#include <gtest/gtest.h>
#include <stdlib.h>

#include <random>
#include <thread>

#include "common/allocator_internal.h"
#include "include/allocator.h"
#include "include/environment.h"
#include "include/pmwcas.h"
#include "include/status.h"
#include "mwcas/mwcas.h"
#include "util/auto_ptr.h"
#include "util/random_number_generator.h"
#ifdef WIN32
#include "environment/environment_windows.h"
#else
#include "environment/environment_linux.h"
#endif

static const uint64_t ARRAY_SIZE = 1024;
static const uint8_t ARRAY_INIT_VALUE = 3;
static const uint32_t UPDATE_ROUND = 1024;
static const uint32_t WORKLOAD_THREAD_CNT = 4;

void ArrayScan(uint64_t* array) {
  for (uint32_t i = 0; i < ARRAY_SIZE; i += 1) {
      
  }
}

namespace pmwcas {
GTEST_TEST(PMwCASTest, SingleThreadedRecovery) {
  auto* descriptor_pool = new pmwcas::DescriptorPool(10000, 2, false);
  auto* allocator_ = (PMDKAllocator*)Allocator::Get();

  uint64_t* array;
  allocator_->Allocate((void**)&array, sizeof(uint64_t) * ARRAY_SIZE);
  memset(array, ARRAY_INIT_VALUE, sizeof(uint64_t) * ARRAY_SIZE);

  auto thread_workload = [&]() {
    std::random_device rd;
    std::mt19937 eng(rd());
    std::uniform_int_distribution<> distr(0, ARRAY_SIZE);

    for (uint32_t i = 0; i < UPDATE_ROUND; i += 1) {
      auto desc = descriptor_pool->AllocateDescriptor();

      /// randomly select array items to perform MwCAS
      for (uint32_t d = 0; d < DESC_CAP; d += 1) {
        auto rng_pos = distr(eng);
        auto item = &array[rng_pos];
        auto old_val = *item;
        desc->AddEntry(item, old_val, old_val + 1);
      }
      desc->MwCAS();
    }
  };

  /// Step 1: start the workload on multiple threads;
  std::thread workers[WORKLOAD_THREAD_CNT];
  for (uint32_t t = 0; t < WORKLOAD_THREAD_CNT; t += 1) {
    workers[t] = std::thread(thread_workload);
  }

  /// Step 2: wait for some time;
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  /// Step 3: force kill all running threads without noticing them
  for (uint32_t t = 0; t < WORKLOAD_THREAD_CNT; t += 1) {
    /// TODO(hao): this only works on Linux
    pthread_cancel(workers[t].native_handle());
  }

  ArrayScan(array);

  descriptor_pool->Recovery(true);

}
}  // namespace pmwcas

int main(int argc, char** argv) {
  google::InitGoogleLogging(argv[0]);
  ::testing::InitGoogleTest(&argc, argv);
  FLAGS_minloglevel = 2;

#ifndef PMDK
  static_assert(false, "PMDK is currently required for recovery");
#endif

  pmwcas::InitLibrary(pmwcas::PMDKAllocator::Create(
                          "mwcas_test_pool", "mwcas_linked_layout",
                          static_cast<uint64_t>(1024) * 1024 * 1204 * 1),
                      pmwcas::PMDKAllocator::Destroy,
                      pmwcas::LinuxEnvironment::Create,
                      pmwcas::LinuxEnvironment::Destroy);
  return RUN_ALL_TESTS();
}
