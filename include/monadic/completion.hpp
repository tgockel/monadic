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
 *    no_value -> has_value    [label="set_result"]
 *    has_callback -> complete [label="set_result"]
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

template <typename T>
class completion
{
public:
    using promise_type = completion_promise<T>;
    
public:
    completion_state state() const
    {
        return impl_->state_;
    }
    
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
    
    T get()
    {
        std::promise<exceptional<T>> promise;
        on_complete([&promise] (exceptional<T>&& src) { promise.set_value(std::move(src)); });
        return promise.get_future().get().get();
    }
    
    void disable()
    {
        unique_lock lock(impl_->protect_);
        impl_->state_ = completion_state::disabled;
        impl_->callback_ = nullptr;
    }
    
    template <typename Func>
    auto map(Func&& func)
            -> completion<decltype(std::declval<exceptional<T>>().map(std::forward<Func>(func)).get())>
    {
        return wrap_call(std::forward<Func>(func), &exceptional<T>::map);
    }
    
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

template <typename T>
class completion_promise
{
public:
    completion_promise() :
            completion_promise(std::make_shared<completion_data<T>>())
    { }
    
    explicit completion_promise(std::shared_ptr<completion_data<T>> impl) :
            impl_(std::move(impl))
    { }
    
    completion<T> get_completion()
    {
        return completion<T>(impl_);
    }
    
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
    
    template <typename... U>
    void set_value(U&&... args)
    {
        complete(exceptional<T>::success(std::forward<U>(args)...));
    }
    
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
