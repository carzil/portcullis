#pragma once

#include <cstdint>
#include <iostream>

#ifdef ASAN_BUILD
extern "C" {
    void __sanitizer_start_switch_fiber(void** fake_stack_save, const void* bottom, size_t size);
    void __sanitizer_finish_switch_fiber(void* fake_stack_save, const void** old_bottom, size_t* old_size);
}
#endif

extern "C" {
    [[noreturn]] void __switch_to(void*);
    [[nodiscard]] uint64_t __save_context(void*);
}

struct TSavedContext {
    uint64_t Registers[8];

    inline void SetStackPointer(void* sp) {
        Registers[6] = reinterpret_cast<uint64_t>(sp);
    }

    inline void SetInstructionPointer(void* ip) {
        Registers[7] = reinterpret_cast<uint64_t>(ip);
    }

    void SwitchTo(TSavedContext& other, void* newStack, size_t newSize, bool exitOld) {
        if (__save_context(&Registers) != 0) {
#ifdef ASAN_BUILD
            __sanitizer_finish_switch_fiber(FakeStack_, &other.StackAddr_, &other.StackSize_);
#endif
            return;
        } else {
#ifdef ASAN_BUILD
            other.PrevContext_ = this;
            if (!exitOld) {
                __sanitizer_start_switch_fiber(&other.FakeStack_, newStack, newSize);
            } else {
                if (!other.IsMain_) {
                    __sanitizer_start_switch_fiber(nullptr, newStack, newSize);
                } else {
                    __sanitizer_start_switch_fiber(nullptr, other.StackAddr_, other.StackSize_);
                }
            }
#endif
            __switch_to(&other.Registers);
        }
    }

    void OnStart() {
#ifdef ASAN_BUILD
        __sanitizer_finish_switch_fiber(FakeStack_, &PrevContext_->StackAddr_, &PrevContext_->StackSize_);
#endif
    }

    void MarkAsMain() {
#ifdef ASAN_BUILD
        IsMain_ = true;
#endif
    }

#ifdef ASAN_BUILD
private:
    void* FakeStack_ = nullptr;
    const void* StackAddr_ = nullptr;
    size_t StackSize_ = 0;
    TSavedContext* PrevContext_ = nullptr;
    bool IsMain_ = false;
#endif
};
