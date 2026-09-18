#ifndef NATIVE_TEST_SUPPORT_H
#define NATIVE_TEST_SUPPORT_H

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdint.h>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

extern unsigned testChecks;

inline void check(bool condition, const char* expression, const char* file, int line)
{
    ++testChecks;
    if (!condition) {
        std::cerr << file << ':' << line << ": CHECK failed: " << expression << '\n';
        std::exit(1);
    }
}

#define CHECK(expression) check(!!(expression), #expression, __FILE__, __LINE__)

template <typename Actual, typename Expected>
void checkEqual(Actual actual, Expected expected, const char* expression, const char* file, int line)
{
    ++testChecks;
    if (actual != expected) {
        std::cerr << file << ':' << line << ": " << expression << " expected " << +expected << ", got " << +actual
                  << '\n';
        std::exit(1);
    }
}

#define CHECK_EQ(actual, expected) checkEqual((actual), (expected), #actual, __FILE__, __LINE__)

// The byte immediately after the requested payload is inaccessible. Any read
// beyond the actual capacity terminates the test process, even without sanitizers.
class GuardedPayload
{
  public:
    explicit GuardedPayload(size_t length) : allocation_(nullptr), pageSize_(0)
    {
#ifdef _WIN32
        SYSTEM_INFO info;
        GetSystemInfo(&info);
        pageSize_ = info.dwPageSize;
        allocation_ =
            static_cast<uint8_t*>(VirtualAlloc(nullptr, pageSize_ * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        CHECK(allocation_ != nullptr);
        DWORD previousProtection = 0;
        CHECK(VirtualProtect(allocation_ + pageSize_, pageSize_, PAGE_NOACCESS, &previousProtection) != 0);
#else
        pageSize_ = static_cast<size_t>(sysconf(_SC_PAGESIZE));
        void* allocation = mmap(nullptr, pageSize_ * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        CHECK(allocation != MAP_FAILED);
        allocation_ = static_cast<uint8_t*>(allocation);
        CHECK(mprotect(allocation_ + pageSize_, pageSize_, PROT_NONE) == 0);
#endif
        CHECK(length <= pageSize_);
        data = allocation_ + pageSize_ - length;
    }

    ~GuardedPayload()
    {
#ifdef _WIN32
        VirtualFree(allocation_, 0, MEM_RELEASE);
#else
        munmap(allocation_, pageSize_ * 2);
#endif
    }

    GuardedPayload(const GuardedPayload&) = delete;
    GuardedPayload& operator=(const GuardedPayload&) = delete;
    uint8_t* data;

  private:
    uint8_t* allocation_;
    size_t pageSize_;
};

void runDecoderTests();
void runCsvTests();

#endif
