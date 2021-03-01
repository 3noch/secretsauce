// Various functions for working with containers of any kind.

#pragma once

#include <boost/optional.hpp>

#include <vector>

// -------- Advanced C++ Trickery ----------------
// From https://stackoverflow.com/a/7943765/503377
template <typename T>
struct function_traits : public function_traits<decltype(&T::operator())>
{
};
// For generic types, directly use the result of the signature of its 'operator()'

template <typename ClassType, typename ReturnType, typename... Args>
struct function_traits<ReturnType (ClassType::*)(Args...) const>
// we specialize for pointers to member function
{
    enum
    {
        arity = sizeof...(Args)
    };
    // arity is the number of arguments.

    typedef ReturnType result_type;

    template <size_t i>
    struct arg
    {
        typedef typename std::tuple_element<i, std::tuple<Args...>>::type type;
        // the i-th argument is equivalent to the i-th tuple element of a tuple
        // composed of those arguments.
    };
};
// ----------------------------------------------

// Map a function over an optional value
template <typename Fn, typename Arg = typename function_traits<Fn>::template arg<0>::type,
          typename Result = typename function_traits<Fn>::result_type>
inline boost::optional<Result> mapped(boost::optional<Arg> const& x, Fn f)
{
    return x ? f(*x) : boost::optional<Result>();
}

// Map a function over a vector
template <typename Fn, typename Arg = typename function_traits<Fn>::template arg<0>::type,
          typename Result = typename function_traits<Fn>::result_type>
inline std::vector<Result> mapped(std::vector<Arg> const& x, Fn f)
{
    std::vector<Result> out;
    out.reserve(x.size());
    std::transform(x.begin(), x.end(), std::back_inserter(out), f);
    return out;
}

// Filter a vector with a predicate. Elements for which the predicate is true are added to the result.
template <typename Fn, typename T = typename function_traits<Fn>::template arg<0>::type>
inline std::vector<T> filtered(std::vector<T> const& v, Fn f)
{
    std::vector<T> out;
    std::copy_if(v.begin(), v.end(), std::back_inserter(out), f);
    return out;
}

// Zips two vectors together using the supplied function. The resulting vector is the same
// length as the shorter of the two input vectors.
template <typename Fn, typename Arg1 = typename function_traits<Fn>::template arg<0>::type,
          typename Arg2 = typename function_traits<Fn>::template arg<1>::type,
          typename Result = typename function_traits<Fn>::result_type>
std::vector<Result> zipWith(std::vector<Arg1> const& as, std::vector<Arg2> const& bs, Fn f)
{
    size_t const shorterSize = std::min(as.size(), bs.size());
    std::vector<Result> out;
    out.reserve(shorterSize);
    for (size_t i = 0; i < shorterSize; i++)
        out.push_back(f(as[i], bs[i]));
    return out;
}
