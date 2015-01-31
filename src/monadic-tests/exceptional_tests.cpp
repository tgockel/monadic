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

#include <monadic/exceptional.hpp>

#include <cassert>

namespace monadic_tests
{

TEST(exceptional_create_convert)
{
    auto l = monadic::exceptional<long>::success(90);
    ensure(l.is_success());
    ensure_eq(90L, l.get());
    monadic::exceptional<int> i = l;
    ensure(l.is_success());
    ensure_eq(90L, l.get());
    ensure(i.is_success());
    ensure_eq(90, i.get());
}

TEST(exceptional_success_map)
{
    monadic::exceptional<long> r = monadic::try_to([] { return 1; })
                                            .map([] (int x) { return x * 2; })
                                            .map([] (long x) { return x * 2; });
    ensure_eq(4L, r.get());
}

TEST(exceptional_success_flatmap)
{
    monadic::exceptional<long> r = monadic::try_to([] { return 1; })
                                            .flatmap([] (int x) { return monadic::exceptional<int>::success(x * 2); })
                                            .flatmap([] (long x) { return monadic::try_to([x] { return x * 2; }); });
    ensure_eq(4L, r.get());
}

TEST(exceptional_failed_map)
{
    auto r = monadic::try_to([] { throw 1; })
                     .map([] { assert(false); return 10; })
                     .map([] (int) { assert(false); });
    ensure(r.is_failure());
    ensure_throws(int, r.get());
}

TEST(exceptional_success_fails_in_map)
{
    auto r = monadic::try_to([] { return 5; })
                     .map([] (int x) { throw x; });
    ensure(r.is_failure());
    ensure_throws(int, r.get());
}

TEST(exceptional_failure_throws_lvalue)
{
    monadic::exceptional<int> i = monadic::try_to([] () -> int { throw 10; });
    ensure_throws(int, i.get());
    const auto& ci = i;
    ensure_throws(int, ci.get());
}

TEST(exceptional_failure_throws_xvalue)
{
    ensure_throws(int, monadic::try_to([] () -> int { throw 10; }).get());
}

}
