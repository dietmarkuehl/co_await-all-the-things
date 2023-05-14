// task.cpp                                                           -*-C++-*-
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
#include <memory>
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

struct task {
    struct promise_type {
        struct final_awaiter {
            promise_type* promise;
            bool await_ready() const noexcept { return false; }
            void await_suspend(std::coroutine_handle<>) noexcept {
                promise->continuation.resume();
            }
            void await_resume() noexcept {}
        };
        std::coroutine_handle<> continuation = std::noop_coroutine();
        std::exception_ptr error;
        track t{"**** promise"};
        std::suspend_always initial_suspend() { return {}; }
        final_awaiter final_suspend() noexcept { return {this}; }
        task get_return_object() { return task{unique_handle(this)}; }
        void unhandled_exception() { error = std::current_exception(); }
        void return_void() {}
        template <typename... T>
        void* operator new(std::size_t s, T&&...) {
            std::cout << "size=" << s << "\n";
            return std::malloc(s);
        }
        void operator delete(void* ptr) { std::free(ptr); }
    };

    using deleter = decltype([](promise_type* p) {
        std::coroutine_handle<promise_type>::from_promise(*p).destroy();
    });
    using unique_handle= std::unique_ptr<promise_type, deleter>;
    unique_handle promise;
    void start() {
        std::coroutine_handle<promise_type>::from_promise(*promise).resume();
    }

    struct nested_awaiter {
        promise_type* promise;
        bool await_ready() const { return false; }
        void await_suspend(std::coroutine_handle<> handle) const {
            promise->continuation = handle;
            std::coroutine_handle<promise_type>::from_promise(*promise).resume();
        }
        void await_resume() {}
    };
    nested_awaiter operator co_await()  {
        return {promise.get()};
    }
};

struct value_awaiter {
    int value;
    bool await_ready() { return true; }
    void await_suspend(auto) {}
    int await_resume() { return value; }
};

struct io {
    std::function<void(std::string const&)> completion;
    void complete(std::string const& line) {
        completion(line);
    }
};

struct async_read {
    io& context;
    std::string line;
    async_read(io& context): context(context) {}
    bool await_ready() { return false; }
    void await_suspend(std::coroutine_handle<> handle) {
        context.completion = [this,handle](std::string line){ 
            this->line = std::move(line);
            handle.resume();
        };
    }
    std::string await_resume() { return line; }
};

task g(io& context) {
    std::cout << "third=" << co_await async_read(context) << "\n";
}

task f(io& context) {
    std::cout << co_await value_awaiter{17} << "\n";
    std::cout << "first=" << co_await async_read(context) << "\n";
    std::cout << "second=" << co_await async_read(context) << "\n";
    co_await g(context);
    std::cout << "f done\n";
}
int main() {
    std::cout << std::unitbuf << std::boolalpha;
    try {
        io context;
        task t = f(context);
        std::cout << "--- after calling f\n";
        t.start();

        context.complete("1st");
        context.complete("2nd");
        context.complete("3rd");
    }
    catch (std::exception const& ex) {
        std::cout << "ERROR: " << ex.what() << "\n";
    }
}
