#pragma once
#include <functional>
#include <memory>
#include <mutex>

namespace nau::PEPEngine::Utils

{
    // Class template for lazy initialization.
    // Copies use reference semantics.
    template <typename T>
    class Lazy
    {
        // Shared state between copies
        struct State
        {
            std::function<T()> createValue;
            std::once_flag initialized;
            std::unique_ptr<T> value;
            bool isInit = false;
        };

    public:
        using value_type = T;

        Lazy() = default;

        explicit Lazy(std::function<T()> createValue)
        {
            state->createValue = createValue;
        }

        explicit operator bool() const
        {
            return static_cast<bool>(state->value);
        }

        T& value()
        {
            init();
            return *state->value;
        }

        const T& value() const
        {
            init();
            return *state->value;
        }

        T* operator->()
        {
            return &value();
        }

        const T* operator->() const
        {
            return &value();
        }

        T& operator*()
        {
            return value();
        }

        const T& operator*() const
        {
            return value();
        }

        bool IsInit() const
        {
            return state->isInit;
        }

    private:
        void init() const
        {
            std::call_once(state->initialized, [&]
            {
                state->value = std::make_unique<T>(state->createValue());
                state->isInit = true;
            });
        }


        std::shared_ptr<State> state = std::make_shared<State>();
    };
}
