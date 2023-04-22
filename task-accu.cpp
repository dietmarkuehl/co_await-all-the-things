// task-accu.cpp                                                           -*-C++-*-
// ----------------------------------------------------------------------------
//  Copyright (C) 2023 Dietmar Kuehl http://www.dietmar-kuehl.de
//
//  Permission is hereby granted, free of charge, to any person
//  obtaining a copy of this software and associated documentation
//  files (the "Software"), to deal in the Software without restriction,
//  including without limitation the rights to use, copy, modify,
//  merge, publish, distribute, sublicense, and/or sell copies of
//  the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be
//  included in all copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
//  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
//  OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
//  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
//  HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
//  WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
//  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
//  OTHER DEALINGS IN THE SOFTWARE.
// ----------------------------------------------------------------------------

#include <concepts>
#include <coroutine>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <list>
#include <optional>
#include <stdexcept>
#include <string.h>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

// ----------------------------------------------------------------------------

struct track {
    char const* name;
    track(char const* name): name(name) { std::cout << name << " ctor\n"; }
    track(track const& other): name(other.name) { std::cout << name << " ctor(copy)\n"; }
    track(track&& other): name(other.name) { std::cout << name << " ctor(move)\n"; }
    ~track() { std::cout << name << " dtor\n"; }
};

// ----------------------------------------------------------------------------

template <typename V>
struct value_awaiter {
    V value;
    constexpr bool await_ready() { return true; }
    void await_suspend(auto) {}
    V await_resume() { return value; }
};

struct is_awaiter_test {
    struct promise_type {
        constexpr std::suspend_always initial_suspend() const { return {}; }
        constexpr std::suspend_always final_suspend() const noexcept { return {}; }
        void unhandled_exception() {}

        is_awaiter_test  get_return_object() { return {}; }
    };
};

template <typename T>
concept is_awaiter
    = requires() { [](T t)->is_awaiter_test { co_await t; }; }
    ;

template <typename R = void>
struct task {
    template <typename V>
    struct base {
        std::optional<V> v;
        void return_value(auto&& r) { v = r; }
        V value() { return *v; }
    };
    template <>
    struct base<void> {
        void return_void() {}
        void value() {}
    };

    struct promise_type: base<R> {
        std::coroutine_handle<> continuation{std::noop_coroutine()};
        std::exception_ptr error{};
        // track t{"****  task"};

        R value() {
            if (error) {
                std::rethrow_exception(error);
            }
            return base<R>::value();
        }

        struct final_awaiter: std::suspend_always {
            promise_type* promise;
            std::coroutine_handle<> await_suspend(auto) noexcept {
                return promise->continuation;
            }
        };
        std::suspend_always initial_suspend() { return {}; }
        final_awaiter final_suspend() noexcept { return {{}, this}; }
        task get_return_object() { return task{unique_promise(this)}; }
        void unhandled_exception() { error = std::current_exception(); }

        template <typename A>
            requires is_awaiter<A>
        auto await_transform(A a) { return a; }
        template <typename V>
            requires (!is_awaiter<V>)
        auto await_transform(V v) { return value_awaiter<V>{v}; }
    };

    using deleter = decltype([](promise_type* p){
        std::coroutine_handle<promise_type>::from_promise(*p).destroy();
    });
    using unique_promise = std::unique_ptr<promise_type, deleter>;
    unique_promise promise;

    void start() {
        auto h= std::coroutine_handle<promise_type>::from_promise(*promise);
        h.resume();
    }
    R value() { return promise->value(); }

    struct nested_awaiter {
        promise_type* promise;
        bool await_ready() const { return false; }
        std::coroutine_handle<> await_suspend(std::coroutine_handle<> continuation) {
            promise->continuation = continuation;
            return std::coroutine_handle<promise_type>::from_promise(*promise);
        }
        R await_resume() { return promise->value(); }
    };
    nested_awaiter operator co_await() { return nested_awaiter{promise.get()}; }
};

struct io {
    std::unordered_map<int, std::function<void(std::string)>> outstanding;
    void submit(int fd, auto fun) {
        outstanding[fd] = fun;
    }
    void complete(int fd, std::string value) {
        auto it = outstanding.find(fd);
        if (it != outstanding.end()) {
            auto fun = it->second;
            outstanding.erase(it);
            fun(value);
        }
    }
};

struct async_read {
    io& context;
    int fd;
    std::string value{};
    constexpr bool await_ready() const { return false; }
    constexpr void await_suspend(std::coroutine_handle<> h) {
        context.submit(fd, [this, h](std::string const& line) {
            value = line;
            h.resume();
        });
    }
    constexpr std::string await_resume() {
        return value;
    }
};

int to_be_made_async() {
    return 17;
}

task<std::string> g(io& c) {
    co_return co_await async_read{c, 1};
}

task<> e() {
    throw std::runtime_error("exception from task");
    co_return;
}

task<> f(io& c) {
    std::cout << "value=" << co_await to_be_made_async() << "\n";
    std::cout << "awaiter=" << co_await async_read{c, 1} << "\n";
    std::cout << "task=" << co_await g(c) << "\n";
    co_await e();
    std::cout << "f end\n";
}
int main() {
    std::cout << std::unitbuf << std::boolalpha;
    try {
        io context;
        auto t = f(context);
        t.start();

        context.complete(1, "first line");
        context.complete(1, "second line");

        t.value();
    }
    catch (std::exception const& ex) {
        std::cout << "ERROR: " << ex.what() << "\n";
    }
}
