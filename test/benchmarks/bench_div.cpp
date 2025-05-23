// intx: extended precision integer library.
// Copyright 2019-2020 Pawel Bylica.
// Licensed under the Apache License, Version 2.0.

#include <benchmark/benchmark.h>
#include <experimental/div.hpp>
#include <intx/intx.hpp>
#include <test/utils/random.hpp>

using namespace intx;

namespace
{
[[gnu::noinline]] uint64_t nop(uint64_t x, uint64_t y) noexcept
{
    return x ^ y;
}

[[gnu::noinline]] uint64_t udiv_native(uint64_t x, uint64_t y) noexcept
{
    return x / y;
}

[[gnu::noinline]] auto reciprocal_2by1_noinline(uint64_t d) noexcept
{
    return reciprocal_2by1(d);
}

[[gnu::noinline]] auto reciprocal_3by2_noinline(uint128 d) noexcept
{
    return reciprocal_3by2(d);
}

inline uint64_t udiv_by_reciprocal(uint64_t uu, uint64_t du) noexcept
{
    auto shift = __builtin_clzl(du);
    auto u = uint128{uu} << shift;
    auto d = du << shift;  // TODO: NOLINT(clang-analyzer-core.BitwiseShift)
    auto v = reciprocal_2by1(d);

    return udivrem_2by1(u, d, v).quot;
}


template <decltype(internal::normalize<512, 512>) NormalizeFn>
void div_normalize(benchmark::State& state)
{
    auto u = uint512{1324254353, 0, 4343242153453, 0, 100324254353, 0, 48882153453, 0};
    auto v = uint512{1333354353, 0, 4343242156663, 0, 16666654353, 0, 48882100453, 0};

    for ([[maybe_unused]] auto _ : state)
    {
        benchmark::ClobberMemory();
        auto x = NormalizeFn(u, v);
        benchmark::DoNotOptimize(x);
    }
}
BENCHMARK(div_normalize<internal::normalize>);

constexpr uint64_t neg(uint64_t x) noexcept
{
    return ~x;
}

template <typename T, uint64_t Fn(T)>
void reciprocal(benchmark::State& state)
{
    auto samples = test::get_samples<T>(test::norm);

    benchmark::ClobberMemory();
    uint64_t x = 0;
    while (state.KeepRunningBatch(test::num_samples))
    {
        for (const auto& i : samples)
            x ^= Fn(i);
    }
    benchmark::DoNotOptimize(x);
}
BENCHMARK(reciprocal<uint64_t, neg>);
BENCHMARK(reciprocal<uint64_t, reciprocal_naive>);
BENCHMARK(reciprocal<uint64_t, reciprocal_2by1>);
BENCHMARK(reciprocal<uint64_t, reciprocal_2by1_noinline>);
BENCHMARK(reciprocal<uint128, reciprocal_3by2>);
BENCHMARK(reciprocal<uint128, reciprocal_3by2_noinline>);

template <uint64_t DivFn(uint64_t, uint64_t)>
void udiv64(benchmark::State& state)
{
    // Pick random operands. Keep the divisor small, because this is the worst
    // case for most algorithms.
    std::mt19937_64 rng{test::get_seed()};
    std::uniform_int_distribution<uint64_t> dist_x;
    std::uniform_int_distribution<uint64_t> dist_y(1, 200);

    constexpr size_t size = 1000;
    std::vector<uint64_t> input_x(size);
    std::vector<uint64_t> input_y(size);
    std::vector<uint64_t> output(size);
    for (auto& x : input_x)
        x = dist_x(rng);
    for (auto& y : input_y)
        y = dist_y(rng);

    while (state.KeepRunningBatch(size))
    {
        for (size_t i = 0; i < size; ++i)
            output[i] = DivFn(input_x[i], input_y[i]);
        benchmark::DoNotOptimize(output.data());
    }

    if (DivFn == nop)
        return;

    // Check results.
    for (size_t i = 0; i < size; ++i)
    {
        if (output[i] != input_x[i] / input_y[i])
        {
            state.SkipWithError("wrong result");
            break;
        }
    }
}

BENCHMARK(udiv64<nop>);
BENCHMARK(udiv64<udiv_by_reciprocal>);
BENCHMARK(udiv64<udiv_native>);
}  // namespace
