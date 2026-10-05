#pragma once

#include "MCTest.h"
#include <deque>
#include <mutex>
#include <tuple>
#include <type_traits>
#include <vector>

/// <summary>
/// A small helper for hand-written mocks: a mock's method forwards its arguments to an <see cref="MCMock::Calls"/>
/// member, which records them, and, for a method with a result, hands back the results the test queued.
/// </summary>
/// <remarks>
/// <code>
/// struct MockSink : Sink
/// {
///     MCMock::Calls&lt;int, std::string&gt; Sent;
///     MCMock::Returning&lt;bool, int&gt; Ready;
///     void Send(int id, std::string text) override { Sent(id, std::move(text)); }
///     bool IsReady(int id) override { return Ready(id); }
/// };
///
/// sink.Ready.Returns(false, true);
/// ...
/// CHECK_CALLED(sink.Sent, 1);
/// CHECK_CALLED_WITH(sink.Sent, 7, "hello");
/// </code>
/// <para>Recording is thread-safe, so a mock may be called from the audio or network threads.</para>
/// </remarks>
namespace MCMock
{
    /// <summary>The calls of one mocked method that returns nothing: each call's arguments, in order.</summary>
    template <typename... Args> class Calls
    {
    public:
        /// <summary>One call's arguments, as stored (references decayed to values).</summary>
        using Call = std::tuple<std::decay_t<Args>...>;

        /// <summary>Records a call.</summary>
        void operator()(Args... args)
        {
            std::lock_guard lock(_Lock);
            _Calls.emplace_back(std::forward<Args>(args)...);
        }

        /// <summary>How many calls were recorded.</summary>
        size_t Count() const
        {
            std::lock_guard lock(_Lock);
            return _Calls.size();
        }

        /// <summary>The arguments of call <paramref name="index"/> (0 = first).</summary>
        Call At(size_t index) const
        {
            std::lock_guard lock(_Lock);
            return _Calls.at(index);
        }

        /// <summary>The arguments of the last call.</summary>
        Call Last() const
        {
            std::lock_guard lock(_Lock);
            return _Calls.back();
        }

        /// <summary>Whether any recorded call had exactly these arguments.</summary>
        template <typename... Expected> bool CalledWith(const Expected&... expected) const
        {
            static_assert(sizeof...(Expected) == sizeof...(Args), "CalledWith takes one value per argument");
            std::lock_guard lock(_Lock);
            return std::ranges::any_of(
                _Calls, [&](const Call& call)
                { return std::apply([&](const auto&... actual) { return ((actual == expected) && ...); }, call); });
        }

        /// <summary>Forgets the recorded calls.</summary>
        void Clear()
        {
            std::lock_guard lock(_Lock);
            _Calls.clear();
        }

    private:
        mutable std::mutex _Lock;
        std::vector<Call> _Calls;
    };

    /// <summary>
    /// The calls of one mocked method with a result: recorded as <see cref="Calls"/> records them, each answered
    /// with the next queued result, or with <paramref name="R"/>'s default once the queue is empty.
    /// </summary>
    template <typename R, typename... Args> class Returning : public Calls<Args...>
    {
    public:
        /// <summary>Queues results for the next calls, in order.</summary>
        template <typename... Results> Returning& Returns(Results&&... results)
        {
            std::lock_guard lock(_ResultLock);
            (_Results.emplace_back(std::forward<Results>(results)), ...);
            return *this;
        }

        /// <summary>Records a call and returns the next queued result.</summary>
        R operator()(Args... args)
        {
            Calls<Args...>::operator()(std::forward<Args>(args)...);
            std::lock_guard lock(_ResultLock);

            if (_Results.empty())
            {
                return R{};
            }

            R result = std::move(_Results.front());
            _Results.pop_front();
            return result;
        }

        /// <summary>Queued results not yet handed out.</summary>
        size_t Pending() const
        {
            std::lock_guard lock(_ResultLock);
            return _Results.size();
        }

    private:
        mutable std::mutex _ResultLock;
        std::deque<R> _Results;
    };
}

/// <summary>Checks that <paramref name="mock"/> recorded exactly <paramref name="count"/> calls.</summary>
#define CHECK_CALLED(mock, count) CHECK_EQ((mock).Count(), static_cast<size_t>(count))

/// <summary>Checks that one of <paramref name="mock"/>'s recorded calls had exactly the given arguments.</summary>
#define CHECK_CALLED_WITH(mock, ...) \
    MCTest::Check((mock).CalledWith(__VA_ARGS__), __FILE__, __LINE__, #mock " called with (" #__VA_ARGS__ ")")
