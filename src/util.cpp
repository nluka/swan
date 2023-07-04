#ifndef SWAN_UTIL_CPP
#define SWAN_UTIL_CPP

#include <iostream>

// Returns the size of a static C-style array at compile time.
template <typename ElemTy, u64 Length>
consteval
u64 lengthof(ElemTy (&)[Length]) { return Length; }

static
void flip_bool(bool &b)
{
    b ^= true;
}

// TODO: fix
char const *strstr_icase(char const *haystack, char const *needle)
{
    u64 needle_len = strlen(needle);
    u64 haystack_len = strlen(haystack);

    for (u64 i = 0; i <= haystack_len - needle_len; ++i) {
        if (
            std::equal(
                needle, needle + needle_len, haystack + i,
                [](char a, char b) { return toupper(a) == toupper(b); }
            )
        ) {
            return haystack + i;
        }
    }

    return nullptr;
}

static
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

static
void format_file_size(
    u64 const file_size,
    char *const out,
    u64 const out_size,
    u64 unit_multiplier
) {
    char const *units[] = { "B", "KB", "MB", "GB", "TB" };
    u64 constexpr largest_unit_idx = (sizeof(units) / sizeof(*units)) - 1;
    u64 unit_idx = 0;

    f64 size = static_cast<double>(file_size);

    while (size >= 1024 && unit_idx < largest_unit_idx) {
        size /= unit_multiplier;
        ++unit_idx;
    }

    char const *const fmt =
        unit_idx == 0
        // no digits after decimal point for bytes
        // because showing a fraction of a byte doesn't make sense
        ? "%.0lf %s"
        // 2 digits after decimal points for denominations
        // greater than bytes
        : "%.2lf %s";

    snprintf(out, out_size, fmt, size, units[unit_idx]);
}

#endif // SWAN_UTIL_CPP
