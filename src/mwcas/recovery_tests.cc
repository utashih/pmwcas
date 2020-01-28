// Copyright (c) Xiangpeng Hao (haoxiangpeng@hotmail.com). All rights reserved.
// Licensed under the MIT license.

#include <gtest/gtest.h>
#include <stdlib.h>

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

namespace pmwcas {
GTEST_TEST(PMwCASTest, SingleThreadedRecovery) {
  auto thread_count = Environment::Get()->GetCoreCount();
  RandomNumberGenerator rng(rand(), 0, kTestArraySize);
  PMwCASPtr* addresses[kWordsToUpdate];
  PMwCASPtr* test_array = nullptr;

  // Create a shared memory segment. This will serve as our test area for
  // the first PMwCAS process that "fails" and will be re-attached to a new
  // descriptor pool and recovered.
  unique_ptr_t<char> memory_segment = alloc_unique<char>(segment_size);
  ::memset(memory_segment.get(), 0, segment_size);
  void* segment_raw = memory_segment.get();

  // Record descriptor count and the initial virtual address of the shm space
  // for recovery later
  DescriptorPool::Metadata* metadata = (DescriptorPool::Metadata*)segment_raw;
  metadata->descriptor_count = kDescriptorPoolSize;
  metadata->initial_address = (uintptr_t)segment_raw;

  DescriptorPool* pool =
      (DescriptorPool*)((char*)segment_raw + sizeof(DescriptorPool::Metadata));

  test_array = (PMwCASPtr*)((uintptr_t)segment_raw + sizeof(DescriptorPool) +
                            sizeof(DescriptorPool::Metadata) +
                            sizeof(Descriptor) * kDescriptorPoolSize);

  // Create a new descriptor pool using an existing memory block, which will
  // be reused by new descriptor pools that will recover from whatever is in the
  // pool from previous runs.
  new (pool) DescriptorPool(kDescriptorPoolSize, thread_count, false);

  for (uint32_t i = 0; i < kTestArraySize; ++i) {
    test_array[i] = 0ull;
  }

  for (uint32_t i = 0; i < kWordsToUpdate; ++i) {
    addresses[i] = nullptr;
  }

  for (uint32_t i = 0; i < kWordsToUpdate; ++i) {
  retry:
    uint64_t idx = rng.Generate();
    for (uint32_t existing_entry = 0; existing_entry < i; ++existing_entry) {
      if (addresses[existing_entry] ==
          reinterpret_cast<PMwCASPtr*>(&test_array[idx])) {
        goto retry;
      }
    }
    addresses[i] = reinterpret_cast<PMwCASPtr*>(&test_array[idx]);
  }

  pool->GetEpoch()->Protect();

  Descriptor* descriptor = pool->AllocateDescriptor();
  EXPECT_NE(nullptr, descriptor);

  for (uint32_t i = 0; i < kWordsToUpdate; ++i) {
    descriptor->AddEntry((uint64_t*)addresses[i], 0ull, 1ull);
  }

  EXPECT_TRUE(descriptor->MwCAS());

  for (uint32_t i = 0; i < kWordsToUpdate; ++i) {
    EXPECT_EQ(1ull, *((uint64_t*)addresses[i]));
  }

  pool->GetEpoch()->Unprotect();
  Thread::ClearRegistry(true);

  // Create a fresh descriptor pool from the previous pools existing memory.
  pool->Recovery(false);

  // The prior MwCAS succeeded, so check whether the pool recovered correctly
  // by ensuring the updates are still present.
  for (uint32_t i = 0; i < kWordsToUpdate; ++i) {
    EXPECT_EQ(1ull, *((uint64_t*)addresses[i]));
  }

  pool->GetEpoch()->Protect();

  descriptor = pool->AllocateDescriptor();
  EXPECT_NE(nullptr, descriptor);

  for (uint32_t i = 0; i < kWordsToUpdate; ++i) {
    descriptor->AddEntry((uint64_t*)addresses[i], 1ull, 2ull);
  }

  EXPECT_FALSE(descriptor->MwCASWithFailure());

  Thread::ClearRegistry(true);

  pool->Recovery(false);
  // Recovery should have rolled back the previously failed pmwcas.
  for (uint32_t i = 0; i < kWordsToUpdate; ++i) {
    EXPECT_EQ(1ull, *((uint64_t*)addresses[i]));
  }

  pool->GetEpoch()->Protect();

  descriptor = pool->AllocateDescriptor();
  EXPECT_NE(nullptr, descriptor);

  for (uint32_t i = 0; i < kWordsToUpdate; ++i) {
    descriptor->AddEntry((uint64_t*)addresses[i], 1ull, 2ull);
  }

  EXPECT_FALSE(descriptor->MwCASWithFailure(0, true));

  Thread::ClearRegistry(true);

  pool->Recovery(false);

  // Recovery should have rolled forward the previously failed pmwcas that made
  // it through the first phase (installing all descriptors).
  for (uint32_t i = 0; i < kWordsToUpdate; ++i) {
    EXPECT_EQ(2ull, *((uint64_t*)addresses[i]));
  }

  Thread::ClearRegistry(true);
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
