#pragma once

#include <stdint.h>

static_assert(sizeof(int8_t) == 1);
typedef int8_t s8;

static_assert(sizeof(int16_t) == 2);
typedef int16_t s16;

static_assert(sizeof(int32_t) == 4);
typedef int32_t s32;

static_assert(sizeof(int64_t) == 8);
typedef int64_t s64;

static_assert(sizeof(uint8_t) == 1);
typedef uint8_t u8;

static_assert(sizeof(uint16_t) == 2);
typedef uint16_t u16;

static_assert(sizeof(uint32_t) == 4);
typedef uint32_t u32;

static_assert(sizeof(uint64_t) == 8);
typedef uint64_t u64;

static_assert(sizeof(size_t) == sizeof(u64));

static_assert(sizeof(float) == 4);
typedef float f32;

static_assert(sizeof(double) == 8);
typedef double f64;

static_assert(sizeof(bool) == 1);
typedef bool b8;
