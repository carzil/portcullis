#include <coro/coro.h>
#include <sys/mman.h>

TCoroStack::TCoroStack(size_t stackSize) {
    Start_ = mmap(nullptr, stackSize + 2 * 4096, PROT_NONE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (Start_ == MAP_FAILED) {
        ThrowErrno("mmap failed");
    }
    int res = mprotect(reinterpret_cast<uint8_t*>(Start_) + 4096, stackSize, PROT_READ | PROT_WRITE);
    if (res == -1) {
        ThrowErrno("mprotect failed");
    }
    Size_ = stackSize;
    Start_ = reinterpret_cast<uint8_t*>(Start_) + 4096;
}

void TCoroStack::Push(uint64_t val) {
    Pos_+= sizeof(uint64_t);
    memcpy(Pointer(), &val, sizeof(uint64_t));
}

TCoroStack::~TCoroStack() {
    munmap(reinterpret_cast<uint8_t*>(Start_) - 4096, Size_ + 2 * 4096);
}
