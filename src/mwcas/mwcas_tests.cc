// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#include <gtest/gtest.h>
#include <stdlib.h>

#include <atomic>

#include "common/allocator_internal.h"
#include "include/allocator.h"
#include "include/environment.h"
#include "include/pmwcas.h"
#include "include/status.h"
#include "mwcas/mwcas.h"
#include "util/auto_ptr.h"
#include "util/random_number_generator.h"
#include "environment/environment_linux.h"

namespace pmwcas {

typedef pmwcas::MwcTargetField<uint64_t> PMwCASPtr;

const uint32_t kDescriptorPoolSize = 0x400;
const uint32_t kTestArraySize = 0x80;
const uint32_t kWordsToUpdate = 4;
const std::string kSharedMemorySegmentName = "mwcastest";

GTEST_TEST(PMwCASTest, SingleThreadedUpdateSuccess) {
  auto thread_count = Environment::Get()->GetCoreCount();

#ifdef PMDK
  auto allocator = reinterpret_cast<PMDKAllocator*>(Allocator::Get());
  auto pool = reinterpret_cast<pmwcas::DescriptorPool*>(
      allocator->GetRoot(sizeof(pmwcas::DescriptorPool)));
  new (pool) pmwcas::DescriptorPool(kDescriptorPoolSize, thread_count);
#else
  auto pool = new pmwcas::DescriptorPool(kDescriptorPoolSize, thread_count);
#endif

  RandomNumberGenerator rng(rand(), 0, kTestArraySize);
  PMwCASPtr test_array[kTestArraySize];
  PMwCASPtr* addresses[kWordsToUpdate];
  uint64_t values[kWordsToUpdate];

  for (uint32_t i = 0; i < kWordsToUpdate; ++i) {
    test_array[i] = 0;
    addresses[i] = nullptr;
    values[i] = 0;
  }

  pool->GetEpoch()->Protect();

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
    values[i] = test_array[idx].GetValueProtected();
  }

  auto descriptor = pool->AllocateDescriptor();
  EXPECT_NE(nullptr, descriptor.GetRaw());

  for (uint32_t i = 0; i < kWordsToUpdate; ++i) {
    descriptor.AddEntry((uint64_t*)addresses[i], values[i], 1ull);
  }

  EXPECT_TRUE(descriptor.MwCAS());

  for (uint32_t i = 0; i < kWordsToUpdate; ++i) {
    EXPECT_EQ(1ull, *((uint64_t*)addresses[i]));
  }

  pool->GetEpoch()->Unprotect();
  Thread::ClearRegistry(true);
}

GTEST_TEST(PMwCASTest, SingleThreadedAbort) {
  auto thread_count = Environment::Get()->GetCoreCount();

#ifdef PMDK
  auto allocator = reinterpret_cast<PMDKAllocator*>(Allocator::Get());
  auto pool = reinterpret_cast<pmwcas::DescriptorPool*>(
      allocator->GetRoot(sizeof(pmwcas::DescriptorPool)));
  new (pool) pmwcas::DescriptorPool(kDescriptorPoolSize, thread_count);
#else
  auto pool = new pmwcas::DescriptorPool(kDescriptorPoolSize, thread_count);
#endif

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
      if (addresses[existing_entry] ==
          reinterpret_cast<PMwCASPtr*>(&test_array[idx])) {
        goto retry;
      }
    }

    addresses[i] = reinterpret_cast<PMwCASPtr*>(&test_array[idx]);
  }

  pool->GetEpoch()->Protect();

  auto descriptor = pool->AllocateDescriptor();
  EXPECT_NE(nullptr, descriptor.GetRaw());

  for (uint32_t i = 0; i < kWordsToUpdate; ++i) {
    descriptor.AddEntry((uint64_t*)addresses[i], 0ull, 1ull);
  }

  EXPECT_TRUE(descriptor.Abort().ok());

  for (uint32_t i = 0; i < kWordsToUpdate; ++i) {
    EXPECT_EQ(0ull, *((uint64_t*)addresses[i]));
  }

  pool->GetEpoch()->Unprotect();
  Thread::ClearRegistry(true);
}

GTEST_TEST(PMwCASTest, SingleThreadedConflict) {
  auto thread_count = Environment::Get()->GetCoreCount();

#ifdef PMDK
  auto allocator = reinterpret_cast<PMDKAllocator*>(Allocator::Get());
  auto pool = reinterpret_cast<pmwcas::DescriptorPool*>(
      allocator->GetRoot(sizeof(pmwcas::DescriptorPool)));
  new (pool) pmwcas::DescriptorPool(kDescriptorPoolSize, thread_count);
#else
  auto pool = new pmwcas::DescriptorPool(kDescriptorPoolSize, thread_count);
#endif

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
      if (addresses[existing_entry] ==
          reinterpret_cast<PMwCASPtr*>(&test_array[idx])) {
        goto retry;
      }
    }

    addresses[i] = reinterpret_cast<PMwCASPtr*>(&test_array[idx]);
  }

  pool->GetEpoch()->Protect();

  auto descriptor = pool->AllocateDescriptor();
  EXPECT_NE(nullptr, descriptor.GetRaw());

  for (uint32_t i = 0; i < kWordsToUpdate; ++i) {
    descriptor.AddEntry((uint64_t*)addresses[i], 0ull, 1ull);
  }

  EXPECT_TRUE(descriptor.MwCAS());

  for (uint32_t i = 0; i < kWordsToUpdate; ++i) {
    EXPECT_EQ(1ull, *((uint64_t*)addresses[i]));
  }

  pool->GetEpoch()->Unprotect();

  pool->GetEpoch()->Protect();

  auto new_descriptor = pool->AllocateDescriptor();
  EXPECT_NE(nullptr, descriptor.GetRaw());

  for (uint32_t i = 0; i < kWordsToUpdate; ++i) {
    new_descriptor.AddEntry((uint64_t*)addresses[i], 0ull, 1ull);
  }

  EXPECT_FALSE(new_descriptor.MwCAS());

  pool->GetEpoch()->Unprotect();
  Thread::ClearRegistry(true);
}

static const uint64_t ARRAY_SIZE = 1024;
static const uint32_t THREAD_COUNT = 6;

void thread_work(uint64_t* array, DescriptorPool* pool,
                 uint64_t time_in_milliseconds) {
  std::random_device rd;
  std::mt19937 eng(rd());
  std::uniform_int_distribution<> distr(0, ARRAY_SIZE - 1);

  auto begin = std::chrono::steady_clock::now();
  uint64_t elapsed = 0;
  while (elapsed < time_in_milliseconds) {
    pmwcas::EpochGuard guard(pool->GetEpoch());
    auto desc = pool->AllocateDescriptor();

    /// generate unique positions
    std::set<uint32_t> positions;
    while (positions.size() < DESC_CAP) {
      positions.insert(distr(eng));
    }

    /// randomly select array items to perform MwCAS
    for (const auto& it : positions) {
      auto item = array + it;
      auto old_val = *item;
      while (!pmwcas::Descriptor::IsCleanPtr(old_val)) {
        old_val = __atomic_load_n(item, __ATOMIC_SEQ_CST);
      }
      desc.AddEntry(item, old_val, old_val + 1);
    }
    desc.MwCAS();

    auto end = std::chrono::steady_clock::now();
    elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin)
                  .count();
  }
  std::cout << "finished all jobs" << std::endl;
}

void ArraySanityCheck(uint64_t* array) {
  uint64_t sum{0};
  for (uint32_t i = 0; i < ARRAY_SIZE; i += 1) {
    sum += array[i];
  }
  ASSERT_EQ(sum % 4, 0);
}

GTEST_TEST(PMwCASTest, MultiThreadedUpdate) {
  uint64_t array[ARRAY_SIZE];
  memset(array, 0, sizeof(uint64_t) * ARRAY_SIZE);
#ifdef PMDK
  auto allocator = reinterpret_cast<PMDKAllocator*>(Allocator::Get());
  auto pool = reinterpret_cast<pmwcas::DescriptorPool*>(
      allocator->GetRoot(sizeof(pmwcas::DescriptorPool)));
  new (pool) pmwcas::DescriptorPool(kDescriptorPoolSize, THREAD_COUNT);
#else
  auto pool = new pmwcas::DescriptorPool(kDescriptorPoolSize, THREAD_COUNT);
#endif

  std::thread workers[THREAD_COUNT];

  /// Suppose the probability of encountering a bug during one iteration is p,
  /// we repeat 20 iterations, and the expectaion of #round encountering a bug
  /// is 20p. For each of such case, probablity of not assert the bug is 25%.
  /// Thus the probablity of not asserting a bug is 0.25^(20p)
  static const uint32_t repeat_times = 20;
  for (uint32_t t = 0; t < repeat_times; t += 1) {
    for (uint32_t i = 0; i < THREAD_COUNT; i += 1) {
      workers[i] = std::thread(thread_work, array, pool, 50);
    }
    for (uint32_t i = 0; i < THREAD_COUNT; i += 1) {
      workers[i].join();
    }
    ArraySanityCheck(array);
    memset(array, 0, sizeof(uint64_t) * ARRAY_SIZE);
  }
}
}  // namespace pmwcas

int main(int argc, char** argv) {
  google::InitGoogleLogging(argv[0]);
  ::testing::InitGoogleTest(&argc, argv);
  FLAGS_alsologtostderr = 1;

#ifdef PMDK
  pmwcas::InitLibrary(pmwcas::PMDKAllocator::Create(
                          "mwcas_test_pool", "mwcas_linked_layout",
                          static_cast<uint64_t>(1024) * 1024 * 1204 * 1),
                      pmwcas::PMDKAllocator::Destroy,
                      pmwcas::LinuxEnvironment::Create,
                      pmwcas::LinuxEnvironment::Destroy);
#else
  pmwcas::InitLibrary(
      pmwcas::DefaultAllocator::Create, pmwcas::DefaultAllocator::Destroy,
      pmwcas::LinuxEnvironment::Create, pmwcas::LinuxEnvironment::Destroy);
#endif
  return RUN_ALL_TESTS();
}
