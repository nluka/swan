#include "stdafx.hpp"
#include "data_types.hpp"
#include "common_functions.hpp"
#include "imgui_dependent_functions.hpp"
#include "path.hpp"

bool basic_dirent::is_path_dotdot()          const noexcept { return path_equals_exactly(path, ".."); }
bool basic_dirent::is_dotdot_dir()           const noexcept { return type == basic_dirent::kind::directory && path_equals_exactly(path, ".."); }
bool basic_dirent::is_directory()            const noexcept { return type == basic_dirent::kind::directory; }
bool basic_dirent::is_symlink()              const noexcept { return one_of(type, { kind::symlink_to_directory, kind::symlink_to_file, kind::invalid_symlink }); }
bool basic_dirent::is_symlink_to_file()      const noexcept { return type == basic_dirent::kind::symlink_to_file; }
bool basic_dirent::is_symlink_to_directory() const noexcept { return type == basic_dirent::kind::symlink_to_directory; }
bool basic_dirent::is_symlink_ambiguous()    const noexcept { return type == basic_dirent::kind::symlink_ambiguous; }
bool basic_dirent::is_file()                 const noexcept { return type == basic_dirent::kind::file; }

char const *basic_dirent::kind_cstr() const noexcept
{
    assert(this->type >= basic_dirent::kind::nil && this->type <= basic_dirent::kind::count);

    switch (this->type) {
        case basic_dirent::kind::directory:            return "directory";
        case basic_dirent::kind::file:                 return "file";
        case basic_dirent::kind::symlink_to_directory: return "symlink_to_directory";
        case basic_dirent::kind::symlink_to_file:      return "symlink_to_file";
        case basic_dirent::kind::symlink_ambiguous:    return "symlink_ambiguous";
        case basic_dirent::kind::invalid_symlink:      return "invalid_symlink";
        default: return "";
    }
}

char const *basic_dirent::kind_short_cstr() const noexcept
{
    assert(this->type >= basic_dirent::kind::nil && this->type <= basic_dirent::kind::count);

    switch (this->type) {
        case basic_dirent::kind::directory:            return "dir";
        case basic_dirent::kind::file:                 return "file";
        case basic_dirent::kind::symlink_to_directory: return ICON_CI_ARROW_SMALL_RIGHT "dir";
        case basic_dirent::kind::symlink_to_file:      return ICON_CI_ARROW_SMALL_RIGHT "file";
        case basic_dirent::kind::symlink_ambiguous:    return ICON_CI_ARROW_SMALL_RIGHT "?";
        case basic_dirent::kind::invalid_symlink:      return ICON_CI_ARROW_SMALL_RIGHT ICON_CI_ISSUE_DRAFT;
        default:                                       return "";
    }
}

char const *basic_dirent::kind_icon() const noexcept
{
    return get_icon(this->type);
}

char const *get_icon(basic_dirent::kind t) noexcept
{
    switch (t) {
        case basic_dirent::kind::directory:            return ICON_FA_FOLDER;
        case basic_dirent::kind::file:                 return ICON_FA_FILE;
        case basic_dirent::kind::symlink_to_directory: return ICON_CI_FILE_SYMLINK_DIRECTORY;
        case basic_dirent::kind::symlink_to_file:      return ICON_CI_FILE_SYMLINK_FILE;
        case basic_dirent::kind::symlink_ambiguous:    return ICON_CI_LINK_EXTERNAL;
        case basic_dirent::kind::invalid_symlink:      return ICON_CI_ERROR;
        default:                                       assert(false && "has no icon"); break;
    }
    return ICON_CI_ERROR;
}

char const *get_icon_for_extension(char const *extension) noexcept
{
    if (extension == nullptr) {
        return ICON_FA_FILE;
    }
    else {
        std::pair<char const *, char const *> const extension_to_icon[] = {
            { "mp3",  ICON_FA_FILE_AUDIO },
            { "wav",  ICON_FA_FILE_AUDIO },
            { "opus", ICON_FA_FILE_AUDIO },
            { "ogg",  ICON_FA_FILE_AUDIO },

            { "csv", ICON_FA_FILE_CSV },

            { "zip", ICON_FA_FILE_ARCHIVE },
            { "rar", ICON_FA_FILE_ARCHIVE },

            { "cpp",  ICON_FA_FILE_CODE },
            { "c",    ICON_FA_FILE_CODE },
            { "hpp",  ICON_FA_FILE_CODE },
            { "h",    ICON_FA_FILE_CODE },
            { "js",   ICON_FA_FILE_CODE },
            { "ts",   ICON_FA_FILE_CODE },
            { "py",   ICON_FA_FILE_CODE },
            { "java", ICON_FA_FILE_CODE },
            { "cs",   ICON_FA_FILE_CODE },

            { "pdf", ICON_FA_FILE_PDF },

            { "doc",  ICON_FA_FILE_WORD },
            { "docx", ICON_FA_FILE_WORD },
            { "pptx", ICON_FA_FILE_POWERPOINT },
            { "ppt",  ICON_FA_FILE_POWERPOINT },
            { "pptx", ICON_FA_FILE_POWERPOINT },
            { "xlsx", ICON_FA_FILE_EXCEL },
            { "xls",  ICON_FA_FILE_EXCEL },
            { "xlsb", ICON_FA_FILE_EXCEL },

            { "mp4",   ICON_FA_FILE_VIDEO },
            { "avi",   ICON_FA_FILE_VIDEO },
            { "mov",   ICON_FA_FILE_VIDEO },
            { "wmv",   ICON_FA_FILE_VIDEO },
            { "mkv",   ICON_FA_FILE_VIDEO },
            { "avchd", ICON_FA_FILE_VIDEO },

            { "jpeg", ICON_FA_FILE_IMAGE },
            { "jpg",  ICON_FA_FILE_IMAGE },
            { "png",  ICON_FA_FILE_IMAGE },
            { "raw",  ICON_FA_FILE_IMAGE },
            { "ico",  ICON_FA_FILE_IMAGE },
            { "tiff", ICON_FA_FILE_IMAGE },
            { "bmp",  ICON_FA_FILE_IMAGE },
            { "pgm",  ICON_FA_FILE_IMAGE },
            { "pnm",  ICON_FA_FILE_IMAGE },
            { "gif",  ICON_FA_FILE_IMAGE },

            { "txt",    ICON_FA_FILE_ALT },
            { "md",     ICON_FA_FILE_ALT },
            { "bat",    ICON_FA_FILE_ALT },
            { "xml",    ICON_FA_FILE_ALT },
            { "ini",    ICON_FA_FILE_ALT },
            { "conf",   ICON_FA_FILE_ALT },
            { "cfg",    ICON_FA_FILE_ALT },
            { "config", ICON_FA_FILE_ALT },
            { "json",   ICON_FA_FILE_ALT },
        };

        for (auto const &pair : extension_to_icon) {
            if (StrCmpI(pair.first, extension) == 0) {
                return pair.second;
            }
        }

        return ICON_FA_FILE;
    }
}

winapi_error get_last_winapi_error() noexcept
{
    DWORD error_code = GetLastError();
    if (error_code == 0) {
        return { 0, "No error." };
    }

    LPSTR buffer = nullptr;
    DWORD buffer_size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        error_code,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPSTR>(&buffer),
        0,
        nullptr
    );

    if (buffer_size == 0) {
        return { 0, "Error formatting message." };
    }

    std::string error_message(buffer, buffer + buffer_size);
    LocalFree(buffer);

    // Remove trailing newline characters
    while (!error_message.empty() && (error_message.back() == '\r' || error_message.back() == '\n')) {
        error_message.pop_back();
    }

    return { error_code, error_message };
}

drive_list_t query_drive_list() noexcept
{
    drive_list_t drive_list;

    s32 drives_mask = GetLogicalDrives();

    for (u64 i = 0; i < 26; ++i) {
        if (drives_mask & (1 << i)) {
            char letter = 'A' + (char)i;

            wchar_t drive_root[] = { wchar_t(letter), L':', L'\\', L'\0' };
            wchar_t volume_name[MAX_PATH + 1];          init_empty_cstr(volume_name);
            wchar_t filesystem_name_utf8[MAX_PATH + 1]; init_empty_cstr(filesystem_name_utf8);
            DWORD serial_num = 0;
            DWORD max_component_length = 0;
            DWORD filesystem_flags = 0;

            auto vol_info_result = GetVolumeInformationW(
                drive_root, volume_name, lengthof(volume_name),
                &serial_num, &max_component_length, &filesystem_flags,
                filesystem_name_utf8, lengthof(filesystem_name_utf8)
            );

            ULARGE_INTEGER total_bytes;
            ULARGE_INTEGER free_bytes;

            if (vol_info_result) {
                auto space_result = GetDiskFreeSpaceExW(drive_root, nullptr, &total_bytes, &free_bytes);
                if (space_result) {
                    drive_info info = {};
                    info.letter = letter;
                    info.total_bytes = total_bytes.QuadPart;
                    info.available_bytes = free_bytes.QuadPart;
                    (void) utf16_to_utf8(volume_name, info.name_utf8, lengthof(info.name_utf8));
                    (void) utf16_to_utf8(filesystem_name_utf8, info.filesystem_name_utf8, lengthof(info.filesystem_name_utf8));
                    drive_list.push_back(info);
                }
            }
        }
    }

    return drive_list;
}

recycle_bin_info query_recycle_bin() noexcept
{
    SHQUERYRBINFO query_info;
    query_info.cbSize = sizeof(query_info);

    recycle_bin_info retval = {};

    retval.result = SHQueryRecycleBinW(nullptr, &query_info);

    if (retval.result == S_OK) {
        retval.bytes_used = query_info.i64Size;
        retval.num_items = query_info.i64NumItems;
    }

    return retval;
}

generic_result reveal_in_windows_file_explorer(swan_path const &full_path_in) noexcept
{
    swan_path full_path_utf8 = full_path_in;

    // Windows File Explorer does not like unix separator
    path_force_separator(full_path_utf8, '\\');

    wchar_t select_path_utf16[MAX_PATH];

    if (!utf8_to_utf16(full_path_utf8.data(), select_path_utf16, lengthof(select_path_utf16))) {
        return { false, "Conversion of path from UTF-8 to UTF-16." };
    }

    std::wstring select_command = {};
    select_command.reserve(1024);

    select_command += L"/select,";
    select_command += L'"';
    select_command += select_path_utf16;
    select_command += L'"';

    WCOUT_IF_DEBUG("select_command: [" << select_command.c_str() << "]\n");

    HINSTANCE result = ShellExecuteW(nullptr, L"open", L"explorer.exe", select_command.c_str(), nullptr, SW_SHOWNORMAL);

    if ((intptr_t)result > HINSTANCE_ERROR) {
        return { true, "" };
    } else {
        return { false, get_last_winapi_error().formatted_message };
    }
}
