#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>
#include <stdbool.h>

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef float f32;
typedef double f64;

typedef uint8_t b8;
typedef uint32_t b32;

#define kilo_bytes(value) ((u32)(value) * 1024)

#endif // UTILS_H