#ifndef SWAN_OPTIONS_HPP
#define SWAN_OPTIONS_HPP

#if defined(NDEBUG)
#   define MAX_EXPLORER_WD_HISTORY 100
#else
#   define MAX_EXPLORER_WD_HISTORY 5 // something small for easier debugging
#endif

struct explorer_options
{
    bool show_cwd_len;
    bool show_debug_info;
    bool automatic_refresh;
    bool show_dotdot_dir;
};

#endif // SWAN_OPTIONS_HPP
