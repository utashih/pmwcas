// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#include <gtest/gtest.h>
#include <stdlib.h>
#include "common/allocator_internal.h"
#include "include/pmwcas.h"
#include "include/environment.h"
#include "include/allocator.h"
#include "include/status.h"
#include "util/random_number_generator.h"
#include "util/auto_ptr.h"
#include "mwcas/mwcas.h"
#ifdef WIN32
#include "environment/environment_windows.h"
#else
#include "environment/environment_linux.h"
#endif

namespace pmwcas {

typedef pmwcas::MwcTargetField<uint64_t> PMwCASPtr;

const uint32_t kDescriptorPoolSize = 0x400;
const uint32_t kTestArraySize = 0x80;
const uint32_t kWordsToUpdate = 4;
const std::string kSharedMemorySegmentName = "mwcastest";

GTEST_TEST(PMwCASTest, SingleThreadedUpdateSuccess) {
  auto thread_count = Environment::Get()->GetCoreCount();
  std::unique_ptr<pmwcas::DescriptorPool> pool(
    new pmwcas::DescriptorPool(kDescriptorPoolSize, thread_count));
  RandomNumberGenerator rng(rand(), 0, kTestArraySize);
  PMwCASPtr test_array[kTestArraySize];
  PMwCASPtr* addresses[kWordsToUpdate];
  uint64_t values[kWordsToUpdate];

  for (uint32_t i = 0; i < kWordsToUpdate; ++i) {
    test_array[i] = 0;
    addresses[i] = nullptr;
    values[i] = 0;
  }

  pool.get()->GetEpoch()->Protect();

  for (uint32_t i = 0; i < kWordsToUpdate; ++i) {
    retry:
    uint64_t idx = rng.Generate();
    for (uint32_t existing_entry = 0; existing_entry < i; ++existing_entry) {
      if (addresses[existing_entry] == reinterpret_cast<PMwCASPtr*>(
          &test_array[idx])) {
        goto retry;
      }
    }

    addresses[i] = reinterpret_cast<PMwCASPtr*>(&test_array[idx]);
    values[i] = test_array[idx].GetValueProtected();
  }

  Descriptor* descriptor = pool->AllocateDescriptor();
  EXPECT_NE(nullptr, descriptor);

  for (uint32_t i = 0; i < kWordsToUpdate; ++i) {
    descriptor->AddEntry((uint64_t*)addresses[i], values[i], 1ull);
  }

  EXPECT_TRUE(descriptor->MwCAS());

  for (uint32_t i = 0; i < kWordsToUpdate; ++i) {
    EXPECT_EQ(1ull, *((uint64_t*)addresses[i]));
  }

  pool.get()->GetEpoch()->Unprotect();
  Thread::ClearRegistry(true);
}

GTEST_TEST(PMwCASTest, SingleThreadedAbort) {
  auto thread_count = Environment::Get()->GetCoreCount();
  std::unique_ptr<pmwcas::DescriptorPool> pool(
    new pmwcas::DescriptorPool(kDescriptorPoolSize, thread_count));
  RandomNumberGenerator rng(rand(), 0, kTestArraySize);
  PMwCASPtr test_array[kTestArraySize];
  PMwCASPtr* addresses[kWordsToUpdate];

  for (uint32_t i = 0; i < kWordsToUpdate; ++i) {
    test_array[i] = 0;
    addresses[i] = nullptr;
  }

  for (uint32_t i = 0; i < kWordsToUpdate; ++i) {
    retry:
    uint64_t idx = rng.Generate();
    for (uint32_t existing_entry = 0; existing_entry < i; ++existing_entry) {
      if (addresses[existing_entry] == reinterpret_cast<PMwCASPtr*>(
          &test_array[idx])) {
        goto retry;
      }
    }

    addresses[i] = reinterpret_cast<PMwCASPtr*>(&test_array[idx]);
  }

  pool.get()->GetEpoch()->Protect();

  Descriptor* descriptor = pool->AllocateDescriptor();
  EXPECT_NE(nullptr, descriptor);

  for (uint32_t i = 0; i < kWordsToUpdate; ++i) {
    descriptor->AddEntry((uint64_t*)addresses[i], 0ull, 1ull);
  }

  EXPECT_TRUE(descriptor->Abort().ok());

  for (uint32_t i = 0; i < kWordsToUpdate; ++i) {
    EXPECT_EQ(0ull, *((uint64_t*)addresses[i]));
  }

  pool.get()->GetEpoch()->Unprotect();
  Thread::ClearRegistry(true);
}

GTEST_TEST(PMwCASTest, SingleThreadedConflict) {
  auto thread_count = Environment::Get()->GetCoreCount();
  std::unique_ptr<pmwcas::DescriptorPool> pool(
    new pmwcas::DescriptorPool(kDescriptorPoolSize, thread_count));
  RandomNumberGenerator rng(rand(), 0, kTestArraySize);
  PMwCASPtr test_array[kTestArraySize];
  PMwCASPtr* addresses[kWordsToUpdate];

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
      if (addresses[existing_entry] == reinterpret_cast<PMwCASPtr*>(
          &test_array[idx])) {
        goto retry;
      }
    }

    addresses[i] = reinterpret_cast<PMwCASPtr*>(&test_array[idx]);
  }

  pool.get()->GetEpoch()->Protect();

  Descriptor* descriptor = pool->AllocateDescriptor();
  EXPECT_NE(nullptr, descriptor);

  for (uint32_t i = 0; i < kWordsToUpdate; ++i) {
    descriptor->AddEntry((uint64_t*)addresses[i], 0ull, 1ull);
  }

  EXPECT_TRUE(descriptor->MwCAS());

  for (uint32_t i = 0; i < kWordsToUpdate; ++i) {
    EXPECT_EQ(1ull, *((uint64_t*)addresses[i]));
  }

  pool.get()->GetEpoch()->Unprotect();

  pool.get()->GetEpoch()->Protect();

  descriptor = pool->AllocateDescriptor();
  EXPECT_NE(nullptr, descriptor);

  for (uint32_t i = 0; i < kWordsToUpdate; ++i) {
    descriptor->AddEntry((uint64_t*)addresses[i], 0ull, 1ull);
  }

  EXPECT_FALSE(descriptor->MwCAS());

  pool.get()->GetEpoch()->Unprotect();
  Thread::ClearRegistry(true);
}
} // namespace pmwcas

int main(int argc, char** argv) {
  google::InitGoogleLogging(argv[0]);
  ::testing::InitGoogleTest(&argc, argv);
  FLAGS_minloglevel = 2;

#ifdef WIN32
  pmwcas::InitLibrary(pmwcas::DefaultAllocator::Create,
                           pmwcas::DefaultAllocator::Destroy,
                           pmwcas::WindowsEnvironment::Create,
                           pmwcas::WindowsEnvironment::Destroy);
#else
#ifdef PMDK
  pmwcas::InitLibrary(pmwcas::PMDKAllocator::Create("mwcas_test_pool",
                                                    "mwcas_linked_layout",
                                                    static_cast<uint64_t >(1024) * 1024 * 1204 * 1),
                      pmwcas::PMDKAllocator::Destroy,
                      pmwcas::LinuxEnvironment::Create,
                      pmwcas::LinuxEnvironment::Destroy);
#else
  pmwcas::InitLibrary(pmwcas::TlsAllocator::Create,
                           pmwcas::TlsAllocator::Destroy,
                           pmwcas::LinuxEnvironment::Create,
                           pmwcas::LinuxEnvironment::Destroy);
#endif
#endif

  return RUN_ALL_TESTS();
}
