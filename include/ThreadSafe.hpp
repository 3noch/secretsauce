#pragma once

#include <mutex>

#include <boost/noncopyable.hpp>

template <typename T>
struct ThreadSafe : private boost::noncopyable
{
    ThreadSafe()
    {
    }
    ThreadSafe(T&& t) : t_(std::move(t))
    {
    }
    ThreadSafe(T const& t) : t_(t)
    {
    }

    void set(T t)
    {
        std::lock_guard<std::mutex> guard(mutex_);
        t_ = t;
    }

    // Makes a copy of the underlying object and returns it.
    // TODO: This is probably not sufficiently correct.
    T getCopy() const
    {
        std::lock_guard<std::mutex> guard(mutex_);
        return T(t_);
    }

    template <typename Result>
    Result with(std::function<Result(T&)> f)
    {
        std::lock_guard<std::mutex> guard(mutex_);
        return f(t_);
    }

    void with(std::function<void(T&)> f)
    {
        std::lock_guard<std::mutex> guard(mutex_);
        return f(t_);
    }

    template <typename Result>
    Result with(std::function<Result(T const&)> f) const
    {
        std::lock_guard<std::mutex> guard(mutex_);
        return f(t_);
    }

    void with(std::function<void(T const&)> f) const
    {
        std::lock_guard<std::mutex> guard(mutex_);
        f(t_);
    }

  private:
    mutable std::mutex mutex_;
    T t_;
};
