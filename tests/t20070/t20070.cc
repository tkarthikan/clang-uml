#include <coroutine>
#include <cstdint>
#include <exception>
#include <iostream>

namespace clanguml::t20070 {

void foo() { }
//
// Based on https://en.cppreference.com/w/cpp/language/coroutines
//

struct AwaitableFoo {
    bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> h) const noexcept { h.resume(); }

    void await_resume() const noexcept { }
};

template <typename T> struct Generator {
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;

    struct promise_type // required
    {
        T value_;
        std::exception_ptr exception_;

        Generator get_return_object()
        {
            return Generator(handle_type::from_promise(*this));
        }

        std::suspend_always initial_suspend() { return {}; }

        std::suspend_always final_suspend() noexcept { return {}; }

        void unhandled_exception()
        {
            exception_ = std::current_exception();
        } // saving
          // exception

        template <std::convertible_to<T> From> // C++20 concept
        std::suspend_always yield_value(From &&from)
        {
            value_ = std::forward<From>(from); // caching the result in promise
            return {};
        }

        void return_void() { }
    };

    handle_type h_;

    Generator(handle_type h)
        : h_(h)
    {
    }

    ~Generator() { h_.destroy(); }

    explicit operator bool()
    {
        fill();
        return !h_.done();
    }

    T operator()()
    {
        fill();
        full_ = false;
        return std::move(h_.promise().value_);
    }

private:
    bool full_ = false;

    void fill()
    {
        if (!full_) {
            h_();
            if (h_.promise().exception_)
                std::rethrow_exception(h_.promise().exception_);

            full_ = true;
        }
    }
};

Generator<unsigned long long> fibonacci_sequence(unsigned n)
{
    if (n == 0) {
        foo();
        co_return;
    }

    if (n > 94)
        throw std::runtime_error(
            "Too big Fibonacci sequence. Elements would overflow.");

    co_await AwaitableFoo{};

    co_yield 0;

    if (n == 1)
        co_return;

    co_yield 1;

    if (n == 2)
        co_return;

    unsigned long long a = 0;
    unsigned long long b = 1;

    for (unsigned i = 2; i < n; ++i) {
        unsigned long long s = a + b;
        co_yield s;
        a = b;
        b = s;
    }
}

int tmain()
{
    try {
        auto gen = fibonacci_sequence(10ULL);

        for (int j = 0; gen; ++j)
            std::cout << "fib(" << j << ")=" << gen() << '\n';
    }
    catch (const std::exception &ex) {
        std::cerr << "Exception: " << ex.what() << '\n';
    }
    catch (...) {
        std::cerr << "Unknown exception.\n";
    }

    return 0;
}
} // namespace clanguml::t20070
