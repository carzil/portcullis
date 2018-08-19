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

    void SwitchTo(TSavedContext& other, void* stack, size_t len) {
        if (__save_context(&Registers) != 0) {
#ifdef ASAN_BUILD
            __sanitizer_finish_switch_fiber(Token_, nullptr, nullptr);
#endif
            return;
        } else {
#ifdef ASAN_BUILD
        __sanitizer_start_switch_fiber(&other.Token_, stack, len);
#endif
            __switch_to(&other.Registers);
        }
    }

    inline void OnStart() {
#ifdef ASAN_BUILD
        __sanitizer_finish_switch_fiber(nullptr, nullptr, nullptr);
#endif
    }

    inline void OnFinish() {
#ifdef ASAN_BUILD
        __sanitizer_start_switch_fiber(nullptr, nullptr, 0);
#endif
    }

#ifdef ASAN_BUILD
private:
    void* Token_ = nullptr;
#endif
};
