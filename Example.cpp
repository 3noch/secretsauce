#include <iostream>

#include "FRP.hpp"

int main()
{
    rx::Event<std::string> msgs;

    {
        auto const x =
            rx::forEach(msgs, [](std::string const& msg) { std::cout << "GOT EVENT: " << msg << std::endl; });

        rx::Event<std::string> mappedEvt = mappedOptional(
            msgs, [](std::string const& m) -> boost::optional<std::string> { return std::string("same"); });

        rx::Dynamic<size_t> msgsCounted = count(msgs);
        auto const countsub =
            rx::forEach(msgsCounted.updated(), [](size_t c) { std::cout << "Counted: " << c << std::endl; });

        auto const countedFiltered = filtered(msgsCounted.updated(), [](size_t i) { return i % 2 == 0; });
        auto const countfiltsub =
            rx::forEach(countedFiltered, [](size_t c) { std::cout << "Filtered count: " << c << std::endl; });

        auto const y = rx::forEach(
            mappedEvt, [=](std::string const& msg) { std::cout << "GOT MAPPED EVENT: " << msg << std::endl; });

        auto const xs = rx::collect(mappedEvt);

        rx::Dynamic<std::string> dyn = rx::mkDynamic<std::string>(msgs, "unset");

        msgs.fire("Hello");
        msgs.fire("There");

        std::cout << "DYN CURRENT: " << dyn.current() << std::endl;

        msgs.fire("Cool!!");

        //std::cout << "ALL: " << xs.result() << std::endl;
    }

    msgs.fire("no subscribers");
    return 0;
}
