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

/** A mutex type which spins on an atomic bool instead of relying on OS functions. When your work unit takes less time
 *  than your OS's quantum and your lock has low contention, a spin mutex can be faster than a regular mutex.
**/
class spin_mutex
{
public:
    using native_handle_type = std::atomic<bool>*;
    
public:
    spin_mutex() :
            locked(false)
    { }
    
    spin_mutex(const spin_mutex&) = delete;
    spin_mutex(spin_mutex&&)      = delete;
    spin_mutex& operator=(const spin_mutex&) = delete;
    spin_mutex& operator=(spin_mutex&&)      = delete;
    
    /** Attempt to lock this mutex. When the lock is acquired, there is a sequentially-consistent guarantee. If the lock
     *  is not acquired, there is no memory ordering guarantee.
     *  
     *  \returns \c true if the lock was obtained; \c false if it was not (someone else holds the lock).
    **/
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
    
    /** Attempt to acquire the lock on this mutex until the specified \a expiry_time. If the given time is in the past,
     *  this function will still attempt to acquire the lock once (this prevents the lock from becoming unobtainable on
     *  slow machines with short spin lengths).
     *  
     *  \param expiry_time The absolute time to give up spinning at. You most likely want to use a monotonic clock
     *    (such as \c std::chrono::steady_clock) for this so you are not subject to NTP or other clock changes.
     *  
     *  \returns \c true if the lock was obtained; \c false if it was not (someone else holds the lock).
    **/
    template <typename TClock, typename TDuration>
    bool try_lock_until(const std::chrono::time_point<TClock, TDuration>& expiry_time)
    {
        do
        {
            if (try_lock())
                return true;
        } while (TClock::now() < expiry_time);
        return false;
    }
    
    /** Attempt to acquire the lock on this mutex for the specified \a duration.
     *  
     *  \param duration The relative time to spin before giving up. The actual ticking time is based on
     *    \c std::chrono::steady_clock.
     *  
     *  \returns \c true if the lock was obtained; \c false if it was not (someone else holds the lock).
    **/
    template <typename TRep, typename TPeriod>
    bool try_lock_for(const std::chrono::duration<TRep, TPeriod>& duration)
    {
        return try_lock_until(std::chrono::steady_clock::now() + duration);
    }
    
    /** Attempt to acquire the lock for the specified number of \a spins.
     *  
     *  \param spins The number of times to call \c try_lock before giving up.
     *  
     *  \returns \c true if the lock was obtained; \c false if it was not (someone else holds the lock).
    **/
    bool try_lock_spins(std::size_t spins)
    {
        while (spins --> 0)
            if (try_lock())
                return true;
        return false;
    }
    
    /** Repeatedly attempt to acquire a lock on this mutex in a while loop. **/
    void lock()
    {
        while (!try_lock())
            continue;
    }
    
    /** Unlock this mutex. There is no checking that you actually own the mutex. **/
    void unlock()
    {
        locked.store(false, std::memory_order_seq_cst);
    }
    
    /** Get a pointer to the atomic variable that backs this mutex. Please do not edit it. **/
    native_handle_type native_handle()
    {
        return &locked;
    }
    
private:
    std::atomic<bool> locked;
};

}

#endif/*__MONADIC_SPIN_MUTEX_HPP_INCLUDED__*/
