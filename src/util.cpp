#ifndef SWAN_UTIL_CPP
#define SWAN_UTIL_CPP

#include <iostream>

void flip_bool(bool &b)
{
    b ^= true;
}

void debug_log([[maybe_unused]] char const *fmt, ...)
{
#if !defined(NDEBUG)
    va_list args;
    va_start(args, fmt);

    IM_ASSERT(vprintf(fmt, args) > 0);

    va_end(args);

    (void) putc('\n', stdout);
#endif
}

static
i32 directory_exists(char const *path)
{
    DWORD attributes = GetFileAttributesA(path);
    return (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY));
}

#endif // SWAN_UTIL_CPP
