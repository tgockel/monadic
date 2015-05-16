/** \file
 *  
 *  Copyright (c) 2015 by Travis Gockel. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
 *  as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
 *  version.
 *
 *  \author Travis Gockel (travis@gockelhut.com)
**/
#include "test.hpp"
#include "util.hpp"

#include <monadic/completion.hpp>
#include <monadic/spin_mutex.hpp>

#include <cassert>
#include <chrono>
#include <thread>

namespace monadic_tests
{

using namespace monadic;

TEST(completion_inline)
{
    completion_promise<int> promise;
    promise.set_value(1);
    completion<int> c = promise.get_completion();
    ensure(c.state() == completion_state::has_value);
    ensure_eq(1, c.get());
    ensure(c.state() == completion_state::complete);
}

TEST(completion_map)
{
    completion_promise<int> promise;
    completion<int> fval = promise.get_completion();
    std::size_t iterations = 20;
    for (std::size_t idx = 0; idx < iterations; ++idx)
        fval = fval.map([] (int x) { return x*2; });
    promise.set_value(1);
    ensure_eq(fval.get(), 1 << iterations);
}

TEST(completion_void_blocked)
{
    completion_promise<void> promise;
    completion<void> val = promise.get_completion();
    bool thread_started = false;
    bool got_value = false;
    std::thread waiter([&]
        {
            thread_started = true;
            val.get();
            got_value = true;
        });
    // spin a bit to make sure the thread actually starts...
    while (!thread_started)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    
    promise.set_value();
    ensure(loop_until([&] { return got_value; }));
    
    waiter.join();
}

TEST(completion_disabled_does_nothing)
{
    completion_promise<void> promise;
    completion<void> val = promise.get_completion();
    bool got_callback = false;
    val.on_complete([&got_callback] (exceptional<void>) { got_callback = true; });
    val.disable();
    promise.set_value();
    ensure(!got_callback);
}

TEST(completion_promise_multiple_sets)
{
    completion_promise<void> promise;
    completion<void> val = promise.get_completion();
    bool got_callback = false;
    val.on_complete([&got_callback] (exceptional<void>) { got_callback = true; });
    promise.set_value();
    ensure(got_callback);
    ensure_throws(std::logic_error, promise.set_value());
}

}
