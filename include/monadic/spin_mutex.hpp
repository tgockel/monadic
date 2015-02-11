/** \file
 *  Header file for \c spin_mutex.
 *  
 *  Copyright (c) 2015 by Travis Gockel. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
 *  as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
 *  version.
 *
 *  \author Travis Gockel (travis@gockelhut.com)
**/
#ifndef __MONADIC_SPIN_MUTEX_HPP_INCLUDED__
#define __MONADIC_SPIN_MUTEX_HPP_INCLUDED__

#include <atomic>
#include <chrono>

namespace monadic
{

class spin_mutex
{
public:
    using native_handle_type = std::atomic<bool>*;
    
public:
    spin_mutex() :
            locked(false)
    { }
    
    spin_mutex(const spin_mutex&) = delete;
    spin_mutex& operator=(const spin_mutex&) = delete;
    
    bool try_lock()
    {
        bool hopeful_val = false;
        return std::atomic_compare_exchange_strong_explicit(&locked,
                                                            &hopeful_val,
                                                            true,
                                                            std::memory_order_seq_cst,
                                                            std::memory_order_relaxed
                                                           );
    }
    
    template <typename TClock, typename TDuration>
    bool try_lock_until(const std::chrono::time_point<TClock, TDuration>& expiry_time)
    {
        while (TClock::now() < expiry_time)
            if (try_lock())
                return true;
        return false;
    }
    
    template <typename TRep, typename TPeriod>
    bool try_lock_for(const std::chrono::duration<TRep, TPeriod>& duration)
    {
        return try_lock_until(std::chrono::steady_clock::now() + duration);
    }
    
    void lock()
    {
        while (!try_lock())
            continue;
    }
    
    void unlock()
    {
        locked.store(false, std::memory_order_seq_cst);
    }
    
    native_handle_type native_handle()
    {
        return &locked;
    }
    
private:
    std::atomic<bool> locked;
};

}

#endif/*__MONADIC_SPIN_MUTEX_HPP_INCLUDED__*/
