/** \file
 *  Header file for \c completion.
 *  
 *  Copyright (c) 2015 by Travis Gockel. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
 *  as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
 *  version.
 *
 *  \author Travis Gockel (travis@gockelhut.com)
**/
#ifndef __MONADIC_COMPLETION_HPP_INCLUDED__
#define __MONADIC_COMPLETION_HPP_INCLUDED__

#include "exceptional.hpp"
#include "scope_exit.hpp"
#include "spin_mutex.hpp"

#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>

namespace monadic
{

template <typename T> class completion;
template <typename T> class completion_promise;

/** Describes the state of a \c completion or \c completion_promise.
 *  
 *  \dot
 *  digraph completion_state {
 *    
 *    no_value
 *    disabled
 *    broken
 *    has_callback
 *    has_value
 *    complete
 *    
 *    no_value -> disabled     [label="disable"]
 *    no_value -> broken       [label="~completion_promise"]
 *    no_value -> has_callback [label="map, flatmap, ..."]
 *    no_value -> has_value    [label="set_value"]
 *    has_callback -> complete [label="set_value"]
 *    has_value -> complete    [label="map, flatmap, ..."]
 *  }
 *  \enddot
**/
enum class completion_state : unsigned char
{
    no_value,     //!< A \c completion_promise has been created, but neither a value nor a continuation have been set.
    has_value,    //!< The \c completion_promise has been delivered and is waiting for retrieval.
    has_callback, //!< The \c completion has a continuation set, but a value has not been delivered.
    complete,     //!< The value has been delivered and retrieved -- the \c completion is done.
    disabled,     //!< The \c completion has been disabled by the receiver.
    broken,       //!< The \c completion_promise was destroyed with an active \c completion without having set the value.
};

/** Holds data for a \c completion or \c completion_promise. **/
template <typename T>
struct completion_data
{
    spin_mutex                             protect_;
    completion_state                       state_;
    exceptional<T>                         value_;
    std::function<void (exceptional<T>&&)> callback_;
};

/** A \c completion is a monadic version of a \c std::future. It can \e mostly be used as a drop-in replacement for a
 *  \c std::future, with the added benefit of having functions like \c map and \c recover.
**/
template <typename T>
class completion
{
public:
    using promise_type = completion_promise<T>;
    
public:
    /** Get the current state of this instance. **/
    completion_state state() const
    {
        return impl_->state_;
    }
    
    /** Call a given \a func with an <tt>exceptional&lt;T&gt;</tt> when this \c completion is delivered (in either
     *  success or failure).
     *  
     *  \tparam Func <tt>void (*)(exceptional&lt;T&gt;&&)</tt>
    **/
    template <typename Func>
    void on_complete(Func&& func)
    {
        unique_lock lock(impl_->protect_);
        if (impl_->state_ == completion_state::no_value)
        {
            impl_->callback_ = std::forward<Func>(func);
            impl_->state_    = completion_state::has_callback;
        }
        else if (impl_->state_ == completion_state::has_value)
        {
            auto completer = on_scope_exit([this] { impl_->state_ = completion_state::complete; });
            std::forward<Func>(func)(std::move(impl_->value_));
        }
        else
        {
            throw std::logic_error("invalid state to call completion::on_complete");
        }
    }
    
    /** Blocks waiting on the result of this completion. If the completion was completed in success, the value will be
     *  returned (on \c void, nothing is returned). If the completion was completed in failure, the exception will be
     *  thrown.
     *  
     *  \note
     *  This is \e not the intended use for a \c completion and only exists for convenient compatibility with
     *  \c std::future. While the majority of \c completion functions are implemented using spin locks, this function is
     *  expected to block, so it relies on \c std::promise and \c std::future, which are slow by comparison.
    **/
    T get()
    {
        std::promise<exceptional<T>> promise;
        on_complete([&promise] (exceptional<T>&& src) { promise.set_value(std::move(src)); });
        return promise.get_future().get().get();
    }
    
    /** Disables this \c completion. It cannot be un-disabled! This is useful for notifying the thread fulfilling the
     *  promise that the receiver no longer cares about the value and is free to abandon processing.
    **/
    void disable()
    {
        unique_lock lock(impl_->protect_);
        impl_->state_ = completion_state::disabled;
        impl_->callback_ = nullptr;
    }
    
    /** Perform the next step of the process when the value is delivered in success. The \a func is only called in the
     *  success case (see \c recover for the failure case).
     *  
     *  \tparam Func <tt>R (*)(T&&)</tt> or <tt>R (*)()</tt> if \c T is \c void; \c R can be \c void
     *  \param  func This function takes in the successful result of this completion and returns another value. The
     *               result of this function will be delivered to the \c completion_promise that backs the \c completion
     *               returned from this function.
     *  \returns a \c completion that will be completed with the result of \a func.
     *  
     *  \example completion_chaining completion chaining
     *  
     *  \code
     *  completion_promise<int> p;
     *  completion<int>         c1 = p.get_completion();
     *  completion<double>      c2 = c1.map([] (int x) { return double(x); });
     *  completion<std::string> c3 = c2.map([] (double d) { return to_string(d); );
     *  completion<void>        c4 = c3.map([] (std::string s) { std::cout << s; });
     *  completion<int>         c5 = c4.map([] () { std::cout << std::endl; return 1; });
     *  p.set_value(1);
     *  c5.get();
     *  \endcode
     *  
     *  \see recover
    **/
    template <typename Func>
    auto map(Func&& func)
            -> completion<decltype(std::declval<exceptional<T>>().map(std::forward<Func>(func)).get())>
    {
        return wrap_call(std::forward<Func>(func), &exceptional<T>::map);
    }
    
    /** Perform the next step of the process if the value is delivered in failure. The \a func is only called in the
     *  failure case (see \c map for the success case).
     *  
     *  \tparam Func <tt>R (*)(std::exception_ptr&&)</tt>; \c R can be \c void
     *  \param  func This function takes in an \c std::exception_ptr and returns some result to continue the chain with.
     *               The result of this function will be delivered to the \c completion_promise that backs the
     *               \c completion returned from this function. If the result of \a func is non-<tt>void</tt>, it must
     *               be convertable to \c T -- the resulting completion is the common type of \c T and \c R. If the
     *               result of \c func is \c void, then \c T must also be \c void.
     *  \returns a \c completion that will be completed with the result of \a func.
     *  
     *  \example
     *  
     *  \code
     *  completion_promise<int> p;
     *  completion<void> c = p.get_completion()
     *                        .map([] (int x) -> int { assert(false && "Will never be called"); return 0; })
     *                        .recover([] (std::exception_ptr ex_ptr)
     *                                 {
     *                                     try
     *                                     {
     *                                         std::rethrow_exception(ex_ptr);
     *                                     }
     *                                     catch (const std::exception& ex)
     *                                     {
     *                                         return std::strlen(ex.what());
     *                                     }
     *                                 })
     *                        // This will be called because we have "recovered":
     *                        .map([] (int x) { return x * 2; })
     *                        .map([] (int x) { std::cout << x << std::endl; });
     *  try
     *  {
     *      throw std::logic_error("Something?");
     *  }
     *  catch (...)
     *  {
     *      p.set_exception(std::current_exception());
     *  }
     *  c.get();
     *  \endcode
    **/
    template <typename Func>
    auto recover(Func&& func)
            -> completion<decltype(std::declval<exceptional<T>>().recover(std::forward<Func>(func)).get())>
    {
        return wrap_call(std::forward<Func>(func), &exceptional<T>::recover);
    }
    
private:
    template <typename U>
    friend class completion_promise;
    
    using unique_lock = std::unique_lock<spin_mutex>;
    
    completion(std::shared_ptr<completion_data<T>> impl) :
            impl_(std::move(impl))
    { }
    
    template <typename TResult, typename Func, typename UFunc>
    completion<TResult> wrap_call(Func&& func, TResult (exceptional<T>::* exfunc)(UFunc&&))
    {
        unique_lock lock(impl_->protect_);
        if (impl_->state_ == completion_state::no_value)
        {
            completion_promise<TResult> result_promise;
            impl_->callback_ = [result_promise, func, exfunc] (exceptional<T>&& result)
                               {
                                   result_promise.complete((std::move(result).*exfunc)(std::move(func)));
                               };
            impl_->state = completion_state::has_callback;
            return result_promise.get_completion();
        }
        else if (impl_->state == completion_state::has_value)
        {
            completion_promise<TResult> result_promise;
            result_promise.complete((std::move(impl_->value_).*exfunc)(std::forward<Func>(func)));
            impl_->state = completion_state::complete;
            return result_promise.get_completion();
        }
        else
        {
            throw std::logic_error("invalid state to continue a completion");
        }
    }
    
private:
    std::shared_ptr<completion_data<T>> impl_;
};

/** A \c completion_promise provides the promise of a delivery of some value to a single \c completion -- fulfilling the
 *  same role of \c std::promise to \c std::future.
**/
template <typename T>
class completion_promise
{
public:
    /** Create a promise value. **/
    completion_promise() :
            completion_promise(std::make_shared<completion_data<T>>())
    { }
    
    /** Create a promise with the given location to store data \a impl. The provided \c completion_data must be uniquely
     *  used for this instance and cannot have been used before.
     *  
     *  \note
     *  This constructor exists to enable bulk allocation of \c completion_data instances in non-critical sections of
     *  code (as memory allocation can be expensive).
    **/
    explicit completion_promise(std::shared_ptr<completion_data<T>> impl) :
            impl_(std::move(impl))
    { }
    
    /** Get a \c completion to back this promise. It is expected to only be called once. **/
    completion<T> get_completion()
    {
        return completion<T>(impl_);
    }
    
    /** Deliver a \a value to this completion. If the associated \c completion has a callback, it is called inline. **/
    template <typename U>
    void complete(exceptional<U> value)
    {
        unique_lock lock(impl_->protect_);
        if (impl_->state_ == completion_state::no_value)
        {
            impl_->value_ = std::move(value);
            impl_->state_ = completion_state::has_value;
        }
        else if (impl_->state_ == completion_state::has_callback)
        {
            auto completer = on_scope_exit([this]
                                           {
                                               impl_->state_ = completion_state::complete;
                                               impl_->callback_ = nullptr;
                                           }
                                          );
            impl_->callback_(std::move(value));
        }
        else if (impl_->state_ == completion_state::disabled)
        {
            // do nothing...
        }
        else
        {
            throw std::logic_error("invalid state for completing a completion_promise");
        }
    }
    
    /** Call \c complete with a successful value. **/
    template <typename... U>
    void set_value(U&&... args)
    {
        complete(exceptional<T>::success(std::forward<U>(args)...));
    }
    
    /** Call \c complete with a failure. **/
    void set_exception(std::exception_ptr ex)
    {
        complete(exceptional<T>::failure(std::move(ex)));
    }
    
private:
    template <typename U>
    friend class completion;
    
    using unique_lock = std::unique_lock<spin_mutex>;
    
private:
    std::shared_ptr<completion_data<T>> impl_;
};

}

#endif/*__MONADIC_COMPLETION_HPP_INCLUDED__*/
