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
#include <iostream>
#include <string>

#include "test.hpp"

int main(int argc, char** argv)
{
    std::string filter;
    if (argc == 2)
        filter = argv[1];
    
    int fail_count = 0;
    for (auto test : monadic_tests::get_unit_tests())
    {
        bool shouldrun = filter.empty()
                      || test->name().find(filter) != std::string::npos;
        if (shouldrun && !test->run())
            ++fail_count;
    }
    
    return fail_count;
}
