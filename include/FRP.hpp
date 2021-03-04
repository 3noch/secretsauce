#pragma once

#include <functional>
#include <memory>

#include <boost/noncopyable.hpp>
#include <boost/optional.hpp>

#include "Containers.hpp"
#include "ThreadSafe.hpp"

namespace rx
{
template <typename In, typename Out>
struct IEvent
{
    using input_type = In;
    using output_type = Out;
    virtual ~IEvent() {}

    virtual void fire(In const& t) = 0;
    virtual void subscribe(std::weak_ptr<std::function<void(Out const&)> const> const& f) = 0;
};

template <typename T>
struct BasicEvent final : public IEvent<T, T>, private boost::noncopyable
{
    void fire(T const& t) override
    {
        bool doCleaning = false;
        for (size_t i = 0; i < subscribers.size(); i++)
        {
            if (subscribers[i].expired())
                doCleaning = true;
            else
                (*subscribers[i].lock())(t);
        }
        if (doCleaning)
            std::remove_if(subscribers.begin(), subscribers.end(),
                           [](std::weak_ptr<std::function<void(T const&)> const> const& x) { return x.expired(); });
    }

    void subscribe(std::weak_ptr<std::function<void(T const&)> const> const& f) override
    {
        subscribers.push_back(f);
    }

    std::vector<std::weak_ptr<std::function<void(T const&)> const>> subscribers;
};

template <typename T>
struct ThreadSafeEvent final : public IEvent<T, T>, private boost::noncopyable
{
    void fire(T const& t) override
    {
        // TODO: Loop over copy of subscribers instead?
        event.with([&](BasicEvent<T>& e) { e.fire(t); });
    }

    void subscribe(std::weak_ptr<std::function<void(T const&)> const> const& f) override
    {
        event.with([&](BasicEvent<T>& e) { e.subscribe(f); });
    }

    ThreadSafe<BasicEvent<T>> event;
};

template <typename T>
struct Event final : public IEvent<T, T>
{
    Event() : event(std::make_shared<ThreadSafeEvent<T>>())
    {
    }

    Event(std::shared_ptr<IEvent<T, T>> e) : event(e)
    {
        assert(e != nullptr);
    }

    void fire(T const& t) override
    {
        event->fire(t);
    }

    void subscribe(std::weak_ptr<std::function<void(T const&)> const> const& f) override
    {
        event->subscribe(f);
    }

    std::shared_ptr<IEvent<T, T>> event;
};

template <typename In, typename Out>
struct ChainedEvent final : public IEvent<Out, Out>
{
    // TODO: Apply functor laws optimization (e.g. fuzed chain-events)
    ChainedEvent(std::shared_ptr<std::function<void(In const&)> const> upstreamSubscription, Event<In> upstream,
                 Event<Out> downstream)
      : upstreamSubscription(upstreamSubscription), upstream(upstream), downstream(downstream)
    {
    }

    void fire(Out const& t) override
    {
        downstream.fire(t);
    }

    void subscribe(std::weak_ptr<std::function<void(Out const&)> const> const& f) override
    {
        downstream.subscribe(f);
    }

    std::shared_ptr<std::function<void(In const&)> const> upstreamSubscription;
    Event<In> upstream;
    Event<Out> downstream;
};

// An event that never fires and therefore has no real subscriptions.
template <typename In, typename Out = In>
struct NeverEvent : public IEvent<In, Out>
{
    void fire(In const& t) override
    {
    }

    void subscribe(std::weak_ptr<std::function<void(Out const&)> const> const& f) override
    {
    }
};

template <typename Fn, typename Upstream = typename function_traits<Fn>::template arg<0>::type,
          typename Downstream = typename function_traits<Fn>::result_type::value_type>
inline Event<Downstream> mappedOptional(Event<Upstream> in, Fn f)
{
    Event<Downstream> downstream;
    std::shared_ptr<ChainedEvent<Upstream, Downstream>> chained = std::make_shared<ChainedEvent<Upstream, Downstream>>(
        std::make_shared<std::function<void(Upstream const&)> const>([=](Upstream const& upstream) mutable {
            boost::optional<Downstream> const result = f(upstream);
            if (result)
                downstream.fire(*result);
        }),
        in, downstream);
    in.subscribe(chained->upstreamSubscription);
    return Event<Downstream>(chained);
}

template <typename Fn, typename Upstream = typename function_traits<Fn>::template arg<0>::type,
          typename Downstream = typename function_traits<Fn>::result_type>
inline Event<Downstream> mapped(Event<Upstream> in, Fn f)
{
    return mappedOptional(in, [=](Upstream const& upstream) { return boost::optional<Downstream>(f(upstream)); });
}

template <typename Fn, typename T = typename function_traits<Fn>::template arg<0>::type>
inline Event<T> filtered(Event<T> in, Fn f)
{
    return mappedOptional(in, [=](T const& t) { return f(t) ? boost::optional<T>(t) : boost::optional<T>(); });
}

template <typename In, typename Out = In>
inline Event<Out> never()
{
    return Event<Out>(std::make_shared<NeverEvent<In, Out>>());
}

template <typename T>
struct ISink
{
    using value_type = T;
    virtual ~ISink() {}

    virtual T result() const = 0;
};

template <typename T>
struct Sink final : public ISink<T>
{
    Sink(std::shared_ptr<ISink<T>> sink) : sink(sink)
    {
        assert(sink != nullptr);
    }

    T result() const override
    {
        return sink->result();
    }
    std::shared_ptr<ISink<T>> const sink;
};

template <typename T>
struct VoidSink final : public ISink<void>
{
    VoidSink(std::shared_ptr<std::function<void(T const&)> const> subscription, Event<T> upstream)
      : subscription(subscription), upstream(upstream)
    {
        assert(subscription != nullptr);
    }
    void result() const override
    {
    }

    std::shared_ptr<std::function<void(T const&)> const> subscription;
    Event<T> upstream;
};

template <typename T>
struct VectorSink final : public ISink<std::vector<T>>
{
    VectorSink(Event<T> upstream)
      : upstream(upstream), subscription(std::make_shared<std::function<void(T const&)> const>([&](T const& t) {
          items.with([&](std::vector<T>& vec) { vec.push_back(t); });
      }))
    {
        upstream.subscribe(subscription);
    }
    std::vector<T> result() const override
    {
        return items.getCopy();
    }

    Event<T> upstream;
    ThreadSafe<std::vector<T>> items;
    std::shared_ptr<std::function<void(T const&)> const> subscription;
};

template <typename Fn, typename In = typename function_traits<Fn>::template arg<0>::type, typename Out = In>
inline Sink<void> forEach(Event<In> in, Fn f)
{
    std::shared_ptr<VoidSink<In>> const sink = std::make_shared<VoidSink<In>>(
        std::make_shared<std::function<void(Out const&)> const>([=](Out const& t) { f(t); }), in);
    in.subscribe(sink->subscription);
    return Sink<void>(sink);
}

template <typename T>
inline Sink<std::vector<T>> collect(Event<T> in)
{
    return Sink<std::vector<T>>(std::make_shared<VectorSink<T>>(in));
}

template <typename T>
struct IDynamic
{
    using value_type = T;
    virtual ~IDynamic() {}

    virtual Event<T> updated() const = 0;
    virtual T current() const = 0;
};

template <typename T>
struct Dynamic final : public IDynamic<T>
{
    Dynamic(std::shared_ptr<IDynamic<T>> d) : dynamic(d)
    {
        assert(d != nullptr);
    }

    Event<T> updated() const override
    {
        return dynamic->updated();
    }
    T current() const override
    {
        return dynamic->current();
    }
    std::shared_ptr<IDynamic<T>> dynamic;
};

template <typename T>
struct BasicDynamic final : public IDynamic<T>, private boost::noncopyable
{
    BasicDynamic(Event<T> e, T initialValue)
      : currentValue(initialValue)
      , updater(std::make_shared<std::function<void(T const&)> const>([&](T const& t) { currentValue.set(t); }))
      , event(e)
    {
    }

    Event<T> updated() const override
    {
        return event;
    }
    T current() const override
    {
        return currentValue.getCopy();
    }

    ThreadSafe<T> currentValue;
    std::shared_ptr<std::function<void(T const&)> const> updater;  // N.B. this may holds references to currentValue so
                                                                   // must be after it
    Event<T> event;  // N.B. this may hold references to other members so much be destructed first
};

template <typename T>
inline Dynamic<T> mkDynamic(Event<T> event, T initialValue)
{
    std::shared_ptr<BasicDynamic<T>> d = std::make_shared<BasicDynamic<T>>(event, initialValue);
    d->event.subscribe(d->updater);
    return Dynamic<T>(d);
};

template <typename Fn, typename T = typename function_traits<Fn>::template arg<0>::type,
          typename Result = typename function_traits<Fn>::result_type>
inline Dynamic<Result> fold(Event<T> event, Result initialValue, Fn f)
{
    std::shared_ptr<BasicDynamic<Result>> d = std::make_shared<BasicDynamic<Result>>(never<Result>(), initialValue);
    BasicDynamic<Result>& selfRef = *d;
    d->event = mapped(event, [&selfRef, f](T const& e) { return f(e, selfRef.current()); });
    d->event.subscribe(d->updater);
    return Dynamic<Result>(d);
}

template <typename T>
inline Dynamic<size_t> count(Event<T> e)
{
    return fold(e, static_cast<size_t>(0), [](T const& _, size_t prev) -> size_t { return prev + 1; });
}

}  // namespace rx
