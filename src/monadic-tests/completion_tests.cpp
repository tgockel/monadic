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

}
