/** \file
 *  Header file for \c exceptional.
 *  
 *  Copyright (c) 2015 by Travis Gockel. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
 *  as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
 *  version.
 *
 *  \author Travis Gockel (travis@gockelhut.com)
**/
#ifndef __MONADIC_EXCEPTIONAL_HPP_INCLUDED__
#define __MONADIC_EXCEPTIONAL_HPP_INCLUDED__

#include <exception>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace monadic
{

template <typename T>
class exceptional;

template <typename T>
struct is_exceptional_type
{
    using type = std::false_type;
};

template <typename T>
struct is_exceptional_type<exceptional<T>>
{
    using type = std::true_type;
};

template <typename T>
using is_exceptional = typename is_exceptional_type<T>::type;

/** A type which might represent either a value or an exception.
 *  
 *  \see try_to
**/
template <typename T>
class exceptional
{
public:
    /** The type of value stored in an \c exceptional instance on success. **/
    using value_type = T;
    
public:
    exceptional() = default;
    
    /** Casting constructor for \c exceptional values. **/
    template <typename U>
    exceptional(exceptional<U> src,
                typename std::enable_if<std::is_convertible<U, T>::value>::type* = nullptr
               ) noexcept(noexcept(T(std::declval<U&&>()))) :
            ex_(std::move(src.ex_)),
            val_(std::move(src.val_))
    { }
    
    /** Forward the \a args to construct a value for this \c exceptional. **/
    template <typename... U>
    explicit exceptional(const std::piecewise_construct_t&, U&&... args)
                noexcept(noexcept(T(std::forward<U>(args)...))) :
            val_(std::forward<U>(args)...)
    { }
    
    /** Create an instance with \c is_success as \c true and a value constructed in-place with \a args. **/
    template <typename... U>
    static exceptional success(U&&... args)
    {
        return exceptional(std::piecewise_construct, std::forward<U>(args)...);
    }
    
    /** Create an instance with \c is_success as \c false and the provided exception value \a ex.
     *  
     *  \throws std::invalid_argument if \a ex is \c nullptr.
    **/
    static exceptional failure(std::exception_ptr ex)
    {
        if (!ex)
            throw std::invalid_argument("exception_ptr must not be null");
        
        exceptional out;
        out.ex_ = std::move(ex);
        return out;
    }
    
    /** Check if this value represents success. If this returns \c true, \c get will not throw. **/
    bool is_success() const noexcept
    {
        return !ex_;
    }
    
    /** Check if this value represents failure. If this returns \c true, \c get will throw. **/
    bool is_failure() const noexcept
    {
        return !!ex_;
    }
    
    /** Get the contained value of this instance if this instance represents success (\c is_success == \c true); throws
     *  the exception if this instance represents failure (\c is_success == \c false).
    **/
    const value_type& get() const &
    {
        if (ex_)
            std::rethrow_exception(ex_);
        else
            return val_;
    }
    
    /** Get the contained value of this instance if this instance represents success (\c is_success == \c true); throws
     *  the exception if this instance represents failure (\c is_success == \c false).
    **/
    value_type& get() &
    {
        if (ex_)
            std::rethrow_exception(ex_);
        else
            return val_;
    }
    
    /** Get the contained value of this instance if this instance represents success (\c is_success == \c true); throws
     *  the exception if this instance represents failure (\c is_success == \c false).
    **/
    value_type&& get() &&
    {
        if (ex_)
            std::rethrow_exception(ex_);
        else
            return std::move(val_);
    }
    
    /** Invoke the provided \a action if this instance represents success and return a new instance with the result of
     *  \a action. If this instance is not successful, return a new instance with the exception. If invoking \a action
     *  throws, the result will also be a failure.
    **/
    template <typename FAction>
    auto map(FAction&& action) const & noexcept
            -> exceptional<decltype(action(std::declval<const value_type&>()))>;
    
    /** Invoke the provided \a action if this instance represents success and return a new instance with the result of
     *  \a action. If this instance is not successful, return a new instance with the exception. If invoking \a action
     *  throws, the result will also be a failure.
    **/
    template <typename FAction>
    auto map(FAction&& action) & noexcept
            -> exceptional<decltype(action(std::declval<value_type&>()))>;
    
    /** Invoke the provided \a action if this instance represents success and return a new instance with the result of
     *  \a action. If this instance is not successful, return a new instance with the exception. If invoking \a action
     *  throws, the result will also be a failure.
    **/
    template <typename FAction>
    auto map(FAction&& action) && noexcept
            -> exceptional<decltype(action(std::declval<value_type&&>()))>;
    
    /** Similar to \c map, but useful if your provided \a action returns an <tt>exceptional&lt;U&gt;</tt>. In this case,
     *  the result is automatically flattened from <tt>exceptional&lt;exceptional&lt;U&gt;&gt;</tt> (if you had used
     *  \c map).
    **/
    template <typename FAction>
    auto flatmap(FAction&& action) const & noexcept
            -> decltype(action(std::declval<const value_type&>()))
    {
        static_assert(is_exceptional<decltype(action(std::declval<const value_type&>()))>::value,
                      "function for flatmap must return an exceptional<U>"
                     );
        return map(std::forward<FAction>(action)).get();
    }
    
    /** Similar to \c map, but useful if your provided \a action returns an <tt>exceptional&lt;U&gt;</tt>. In this case,
     *  the result is automatically flattened from <tt>exceptional&lt;exceptional&lt;U&gt;&gt;</tt> (if you had used
     *  \c map).
    **/
    template <typename FAction>
    auto flatmap(FAction&& action) & noexcept
            -> decltype(action(std::declval<value_type&>()))
    {
        static_assert(is_exceptional<decltype(action(std::declval<value_type&>()))>::value,
                      "function for flatmap must return an exceptional<U>"
                     );
        return map(std::forward<FAction>(action)).get();
    }
    
    /** Similar to \c map, but useful if your provided \a action returns an <tt>exceptional&lt;U&gt;</tt>. In this case,
     *  the result is automatically flattened from <tt>exceptional&lt;exceptional&lt;U&gt;&gt;</tt> (if you had used
     *  \c map).
    **/
    template <typename FAction>
    auto flatmap(FAction&& action) & noexcept
            -> decltype(action(std::declval<value_type&&>()))
    {
        static_assert(is_exceptional<decltype(action(std::declval<value_type&&>()))>::value,
                      "function for flatmap must return an exceptional<U>"
                     );
        return map(std::forward<FAction>(action)).get();
    }
    
    /** Call some \a action if this instance is not success (the opposite of \c map).
     *  
     *  \param action is some function which is given an \c std::exception_ptr and returns a value. It is only called if
     *                \c is_success is \c false.
     *  \returns An instance with a copy of the value in this one if \c is_success is \c true; an instance with the
     *           result of calling \a action if \c is_success if \c false and it returns in success; otherwise, the
     *           result will have the exception thrown from \a action.
    **/
    template <typename FAction>
    auto recover(FAction&& action) const & noexcept
            -> exceptional<typename std::common_type<T, decltype(action(std::exception_ptr()))>::type>;
    
    /** Call some \a action if this instance is not success (the opposite of \c map).
     *  
     *  \param action is some function which is given an \c std::exception_ptr and returns a value. It is only called if
     *                \c is_success is \c false.
     *  \returns An instance with a copy of the value in this one if \c is_success is \c true; an instance with the
     *           result of calling \a action if \c is_success if \c false and it returns in success; otherwise, the
     *           result will have the exception thrown from \a action.
    **/
    template <typename FAction>
    auto recover(FAction&& action) && noexcept
            -> exceptional<typename std::common_type<T, decltype(action(std::exception_ptr()))>::type>;
    
private:
    template <typename U>
    friend class exceptional;
    
private:
    std::exception_ptr ex_;
    value_type         val_;
};

template <>
class exceptional<void>
{
public:
    exceptional() noexcept
    { }
    
    exceptional(const std::piecewise_construct_t&) noexcept
    { }
    
    /** Create an empty, successful instance. **/
    static exceptional success()
    {
        return exceptional();
    }
    
    /** Create an instance with \c is_success as \c false and the provided exception value \a ex.
     *  
     *  \throws std::invalid_argument if \a ex is \c nullptr.
    **/
    static exceptional failure(std::exception_ptr ex)
    {
        if (!ex)
            throw std::invalid_argument("exception_ptr must not be null");
        exceptional out;
        out.ex_ = std::move(ex);
        return out;
    }
    
    /** Throws an exception if this instance represents failure; does nothing if this represents success. **/
    void get() const
    {
        if (ex_)
            std::rethrow_exception(ex_);
    }
    
    /** Check if this value represents success. If this returns \c true, \c get will not throw. **/
    bool is_success() const noexcept
    {
        return !ex_;
    }
    
    /** Check if this value represents failure. If this returns \c true, \c get will throw. **/
    bool is_failure() const noexcept
    {
        return !!ex_;
    }
    
    template <typename FAction>
    auto map(FAction&& action) const noexcept
            -> exceptional<decltype(action())>;
    
    template <typename FAction>
    auto flatmap(FAction&& action) const noexcept
            -> decltype(action())
    {
        static_assert(is_exceptional<decltype(action())>::value, "function for flatmap must return an exceptional<U>");
        return map(std::forward<FAction>(action)).get();
    }
    
    /** Call some \a action if this instance is not success.
     *  
     *  \param action is some function which is given an \c std::exception_ptr and returns \c void. It is only called if
     *                \c is_success is \c false.
     *  \returns An instance that has \c is_success as \c true if \c is_success is \c true or \a action did not throw an
     *           exception; otherwise, the result will have the exception thrown from \a action.
    **/
    template <typename FAction>
    auto recover(FAction&& action) const noexcept -> exceptional<void>;
    
private:
    std::exception_ptr ex_;
};

namespace detail
{

template <typename T>
struct try_to
{
    template <typename FAction, typename... TArgs>
    static exceptional<T> exec(FAction&& action, TArgs&&... args) noexcept
    {
        try
        {
            return exceptional<T>::success(action(std::forward<TArgs>(args)...));
        }
        catch (...)
        {
            return exceptional<T>::failure(std::current_exception());
        }
    }
};

template <>
struct try_to<void>
{
    template <typename FAction, typename... TArgs>
    static exceptional<void> exec(FAction&& action, TArgs&&... args) noexcept
    {
        try
        {
            action(std::forward<TArgs>(args)...);
            return exceptional<void>::success();
        }
        catch (...)
        {
            return exceptional<void>::failure(std::current_exception());
        }
    }
};

}

/** Attempt to execute an \a action with the given \a args.
 *  
 *  \returns On success, the \c exceptional will have the result of the function. On failure, the returned value will be
 *   filled with an exception.
**/
template <typename FAction, typename... TArgs>
auto try_to(FAction&& action, TArgs&&... args) noexcept
        -> exceptional<decltype(action(std::forward<TArgs>(args)...))>
{
    return detail::try_to<decltype(action(std::forward<TArgs>(args)...))>
                     ::exec(std::forward<FAction>(action), std::forward<TArgs>(args)...);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Implementation                                                                                                     //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T>
template <typename FAction>
auto exceptional<T>::map(FAction&& action) const & noexcept
        -> exceptional<decltype(action(std::declval<const value_type&>()))>
{
    if (ex_)
        return exceptional<decltype(action(get()))>::failure(ex_);
    else
        return try_to(std::forward<FAction>(action), get());
}

template <typename T>
template <typename FAction>
auto exceptional<T>::map(FAction&& action) & noexcept
        -> exceptional<decltype(action(std::declval<value_type&>()))>
{
    if (ex_)
        return exceptional<decltype(action(get()))>::failure(ex_);
    else
        return try_to(std::forward<FAction>(action), get());
}

template <typename T>
template <typename FAction>
auto exceptional<T>::map(FAction&& action) && noexcept
        -> exceptional<decltype(action(std::declval<value_type&&>()))>
{
    if (ex_)
        return exceptional<decltype(action(get()))>::failure(ex_);
    else
        return try_to(std::forward<FAction>(action), std::move(val_));
}

template <typename FAction>
auto exceptional<void>::map(FAction&& action) const noexcept
        -> exceptional<decltype(action())>
{
    if (ex_)
        return exceptional<decltype(action())>::failure(ex_);
    else
        return try_to(std::forward<FAction>(action));
}

template <typename T>
template <typename FAction>
auto exceptional<T>::recover(FAction&& action) const & noexcept
        -> exceptional<typename std::common_type<T, decltype(action(std::exception_ptr()))>::type>
{
    using result_type = exceptional<typename std::common_type<T, decltype(action(std::exception_ptr()))>::type>;
    
    try
    {
        if (is_success())
            return *this;
        else
            return result_type::success(std::forward<FAction>(action)(ex_));
    }
    catch (...)
    {
        return result_type::failure(std::current_exception());
    }
}

template <typename T>
template <typename FAction>
auto exceptional<T>::recover(FAction&& action) && noexcept
        -> exceptional<typename std::common_type<T, decltype(action(std::exception_ptr()))>::type>
{
    using result_type = exceptional<typename std::common_type<T, decltype(action(std::exception_ptr()))>::type>;
    
    try
    {
        if (is_success())
            return std::move(*this);
        else
            return result_type::success(std::forward<FAction>(action)(ex_));
    }
    catch (...)
    {
        return result_type::failure(std::current_exception());
    }
}

template <typename FAction>
auto exceptional<void>::recover(FAction&& action) const noexcept -> exceptional<void>
{
    if (is_success())
        return *this;
    else
        return try_to([this, &action] { std::forward<FAction>(action)(ex_); });
}

}
#endif/*__MONADIC_EXCEPTIONAL_HPP_INCLUDED__*/
