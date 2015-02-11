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
#ifndef __MONADIC_TESTS_UTIL_HPP_INCLUDED__
#define __MONADIC_TESTS_UTIL_HPP_INCLUDED__

#include "test.hpp"

#include <chrono>
#include <utility>

namespace monadic_tests
{

template <typename Predicate, typename TClock, typename TDuration>
bool loop_until(Predicate&& pred, const std::chrono::time_point<TClock, TDuration>& expiry)
{
    while (!pred())
    {
        if (TClock::now() > expiry)
            return false;
    }
    return true;
}

template <typename Predicate>
bool loop_until(Predicate&& pred)
{
    return loop_until(std::forward<Predicate>(pred), std::chrono::steady_clock::now() + std::chrono::milliseconds(100));
}

}

#endif/*__MONADIC_TESTS_UTIL_HPP_INCLUDED__*/
