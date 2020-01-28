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
static const uint8_t ARRAY_INIT_VALUE = 0;
static const uint32_t UPDATE_ROUND = 1024;
static const uint32_t WORKLOAD_THREAD_CNT = 4;

void ArrayScan(uint64_t* array) {
  uint64_t dirty_cnt{0}, concas_cnt{0}, mwcas_cnt{0};
  for (uint32_t i = 0; i < ARRAY_SIZE; i += 1) {
    uint64_t val = array[i];
    if ((val & pmwcas::Descriptor::kDirtyFlag) != 0) {
      dirty_cnt += 1;
      val = val & ~pmwcas::Descriptor::kDirtyFlag;
    }
    if ((val & pmwcas::Descriptor::kCondCASFlag) != 0) {
      concas_cnt += 1;
      val = val & ~pmwcas::Descriptor::kCondCASFlag;
    }
    if ((val & pmwcas::Descriptor::kMwCASFlag) != 0) {
      mwcas_cnt += 1;
      val = val & ~pmwcas::Descriptor::kMwCASFlag;
    }
  }
  std::cout << "Dirty count: " << dirty_cnt
            << "\tCondition CAS count: " << concas_cnt
            << "\tMwCAS count: " << mwcas_cnt << std::endl;
}

namespace pmwcas {
GTEST_TEST(PMwCASTest, SingleThreadedRecovery) {
  auto* descriptor_pool = new pmwcas::DescriptorPool(10000, 2, false);
  auto* allocator_ = (PMDKAllocator*)Allocator::Get();

  uint64_t** root_obj = (uint64_t**)allocator_->GetRoot(sizeof(uint64_t));
  allocator_->Allocate((void**)root_obj, sizeof(uint64_t) * ARRAY_SIZE);
  uint64_t* array = *root_obj;
  memset(array, ARRAY_INIT_VALUE, sizeof(uint64_t) * ARRAY_SIZE);

  auto thread_workload = [&]() {
    std::random_device rd;
    std::mt19937 eng(rd());
    std::uniform_int_distribution<> distr(0, ARRAY_SIZE);

    for (uint32_t i = 0; i < UPDATE_ROUND; i += 1) {
      pmwcas::EpochGuard guard(descriptor_pool->GetEpoch());
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
  std::this_thread::sleep_for(std::chrono::milliseconds(300));

  /// Step 3: force kill all running threads without noticing them
  for (uint32_t t = 0; t < WORKLOAD_THREAD_CNT; t += 1) {
    /// TODO(hao): this only works on Linux
    pthread_cancel(workers[t].native_handle());
  }

  ArrayScan(array);

  /// Step 4: perform the recovery
  descriptor_pool->Recovery(false);

  /// Step 5: check every item in the array
  std::map<uint64_t, uint32_t> histogram;
  for (uint32_t i = 0; i < ARRAY_SIZE; i += 1) {
    auto value = array[i];

    ASSERT_TRUE(pmwcas::Descriptor::IsCleanPtr(value));

    if (histogram.find(value) == histogram.end()) {
      histogram[value] = 1;
    } else {
      histogram[value] += 1;
    }
  }
  std::cout << "Printing the array histogram ---------\nvalue\tcount\n";
  for (const auto& item : histogram) {
    std::cout << item.first << "\t" << item.second << std::endl;
  }

  /// Step 6: perform random work over the pool again, there should not be any
  /// error.
  for (uint32_t t = 0; t < WORKLOAD_THREAD_CNT; t += 1) {
    workers[t] = std::thread(thread_workload);
  }
  for (uint32_t t = 0; t < WORKLOAD_THREAD_CNT; t += 1) {
    workers[t].join();
  }
}
}  // namespace pmwcas

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);

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
