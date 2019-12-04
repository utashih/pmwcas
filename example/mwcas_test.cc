#include <iostream>
#include <pmwcas.h>
#include "cassert"
#include <glog/logging.h>

uint64_t *array = nullptr;
pmwcas::DescriptorPool *descriptor_pool = nullptr;

void test()
{
    pmwcas::Allocator::Get()->Allocate((void **)&array, sizeof(uint64_t) * 100);

    memset(array, 0, sizeof(uint64_t) * 100);

    new (descriptor_pool) pmwcas::DescriptorPool(10000, 2, false);
    auto desc = descriptor_pool->AllocateDescriptor();

    desc->AddEntry(&array[0], 0, 1);
    desc->AddEntry(&array[1], 0, 2);
    desc->AddEntry(&array[2], 0, 3);
    desc->AddEntry(&array[3], 0, 4);

    desc->MwCAS();

    CHECK_EQ(array[0], 1);
    CHECK_EQ(array[1], 2);
    CHECK_EQ(array[2], 3);
    CHECK_EQ(array[3], 4);
}

int main()
{
    std::cout << "hello world!" << std::endl;
    pmwcas::InitLibrary(pmwcas::PMDKAllocator::Create("mwcas_pool",
                                                      "mwcas_layout",
                                                      static_cast<uint64_t>(1024) * 1024 * 1204 * 1),
                        pmwcas::PMDKAllocator::Destroy,
                        pmwcas::LinuxEnvironment::Create,
                        pmwcas::LinuxEnvironment::Destroy);
}
