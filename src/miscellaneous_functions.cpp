#include "stdafx.hpp"
#include "data_types.hpp"
#include "common_functions.hpp"
#include "imgui_dependent_functions.hpp"
#include "path.hpp"

std::initializer_list<basic_dirent::kind> const symlink_types = {
    basic_dirent::kind::symlink_to_directory,
    basic_dirent::kind::symlink_to_file,
    basic_dirent::kind::symlink_ambiguous,
    basic_dirent::kind::invalid_symlink
};

bool basic_dirent::is_path_dotdot()          const noexcept { return path_equals_exactly(path, ".."); }
bool basic_dirent::is_dotdot_dir()           const noexcept { return type == kind::directory && path_equals_exactly(path, ".."); }
bool basic_dirent::is_directory()            const noexcept { return type == kind::directory; }
bool basic_dirent::is_symlink()              const noexcept { return one_of(type, symlink_types); }
bool basic_dirent::is_symlink_to_file()      const noexcept { return type == kind::symlink_to_file; }
bool basic_dirent::is_symlink_to_directory() const noexcept { return type == kind::symlink_to_directory; }
bool basic_dirent::is_symlink_ambiguous()    const noexcept { return type == kind::symlink_ambiguous; }
bool basic_dirent::is_file()                 const noexcept { return type == kind::file; }

bool basic_dirent::is_symlink(kind t) noexcept { return one_of(t, symlink_types); }

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

char const *basic_dirent::kind_description(basic_dirent::kind t) noexcept
{
    assert(t >= basic_dirent::kind::nil && t <= basic_dirent::kind::count);

    switch (t) {
        case basic_dirent::kind::nil:                  return "(Nil)";
        case basic_dirent::kind::directory:            return "Directory";
        case basic_dirent::kind::file:                 return "File";
        case basic_dirent::kind::symlink_to_directory: return "Directory Link";
        case basic_dirent::kind::symlink_to_file:      return "File Link";
        case basic_dirent::kind::symlink_ambiguous:    return "Link";
        case basic_dirent::kind::invalid_symlink:      return "Invalid Link";
        case basic_dirent::kind::count:                return "(Count)";
        default: return "";
    }
}

char const *basic_dirent::kind_short_cstr() const noexcept
{
    assert(this->type >= basic_dirent::kind::nil && this->type <= basic_dirent::kind::count);

    switch (this->type) {
        case basic_dirent::kind::directory:            return "dir";
        case basic_dirent::kind::file:                 return "file";
        case basic_dirent::kind::symlink_to_directory: return ICON_CI_ARROW_SMALL_RIGHT "d";
        case basic_dirent::kind::symlink_to_file:      return ICON_CI_ARROW_SMALL_RIGHT "f";
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
        case basic_dirent::kind::directory:            return ICON_LC_FOLDER;
        case basic_dirent::kind::file:                 return ICON_LC_FILE;
        case basic_dirent::kind::symlink_to_directory: return ICON_LC_FOLDER_SYMLINK;
        case basic_dirent::kind::symlink_to_file:      return ICON_LC_FILE_SYMLINK;
        case basic_dirent::kind::symlink_ambiguous:    return ICON_LC_LINK_2;
        case basic_dirent::kind::invalid_symlink:      return ICON_LC_FILE_X_2;
        default:                                       assert(false && "has no icon"); break;
    }
    return ICON_CI_ERROR;
}

std::array<char, 64> get_type_text_for_extension(char const *extension) noexcept
{
    if (extension == nullptr) {
        return make_str_static<64>("(nullptr)");
    }

    using extension_pair_t = std::pair<char const *, char const *>;

    static std::vector<extension_pair_t> const extension_to_type_text = {
        { "1", "RetroPie ROM File" },
        { "1st", "ReadMe File" },
        { "323", "H.323 Internet Telephony File" },
        { "386", "Windows Virtual Device Driver File" },
        { "3ds", "Autodesk 3D Studio Max Scene" },
        { "3g2", "3GPP2 Multimedia File" },
        { "3gp", "3GPP Multimedia File" },
        { "669", "UNIS Composer 669 Module" },
        { "6cm", "Six Channel Module" },
        { "7z", "7-Zip Archive" },
        { "8", "Assembly Language Source Code File" },
        { "8cm", "Eight Channel Module" },
        { "9cm", "Nine Channel Module" },
        { "a", "Archive" },
        { "a3m", "Authorware 3.x Library" },
        { "a3w", "Authorware 3.x Windows File" },
        { "a4m", "Authorware 4.x Library" },
        { "a4w", "Authorware 4.x Windows File" },
        { "a5w", "Authorware 5.x Windows File" },
        { "a6p", "Authorware 6 Program File" },
        { "aac", "Advanced Audio Coding" },
        { "ab", "AppBuilder Source Code" },
        { "abf", "Adobe Binary Screen Font" },
        { "abk", "Automatic Backup File" },
        { "abm", "WordPerfect Address Book File" },
        { "abw", "AbiWord Document" },
        { "aca", "Agent Character Animation" },
        { "accdb", "Microsoft Access Database" },
        { "acf", "Audio Configuration File" },
        { "acr", "ACRobot Script" },
        { "acsm", "Adobe Content Server Message" },
        { "act", "ActionScript File" },
        { "ad", "After Dark Screensaver" },
        { "ada", "Ada Source Code" },
        { "adb", "Android Debug Bridge Command File" },
        { "adc", "Scanstudio 16 Color Bitmap Graphic" },
        { "add", "Dynamics AX Developer Documentation File" },
        { "adf", "Amiga Disk File" },
        { "adn", "Access Blank Project Template" },
        { "adr", "AfterDark Random Screen Saver Module" },
        { "adt", "Windows CardSpace File" },
        { "adx", "Approach Index File" },
        { "afa", "Astrotite 200X Archive" },
        { "afi", "Asterisk Configuration File" },
        { "afl", "FontLab Font File" },
        { "afm", "Adobe Font Metrics File" },
        { "afo", "ArchestrA Graphic File Object" },
        { "afs", "Adobe Flash Project File" },
        { "ag", "Applix Graphic" },
        { "agp", "AgileGraph Plug-in" },
        { "ai", "Adobe Illustrator Artwork" },
        { "aif", "Audio Interchange File Format" },
        { "aiff", "Audio Interchange File Format" },
        { "aim", "AOL Instant Messenger File" },
        { "air", "Adobe AIR Installation Package" },
        { "akp", "Akai Sampler File" },
        { "al", "A-Law Compressed Sound Format" },
        { "alb", "Alpha Five Library" },
        { "alb", "JASC Image Commander Album" },
        { "alz", "ALZip Archive" },
        { "am", "Automake Makefile Template" },
        { "amf", "Action Message Format File" },
        { "ami", "Velvet Studio Music Module" },
        { "amr", "Adaptive Multi-Rate Audio Codec" },
        { "amr", "Adaptive Multi-Rate Codec File" },
        { "ams", "Velvet Studio Music Module" },
        { "amu", "PictureGear Studio Photo Album" },
        { "an", "Allegro Noise Tracker Module" },
        { "ani", "Windows Animated Cursor" },
        { "anm", "Computer Associates Network Model" },
        { "ans", "ANSI Text File" },
        { "aoi", "Art of Illusion Scene" },
        { "ap", "ArtPro File" },
        { "apc", "Adobe Presenter Project" },
        { "ape", "AVS Plugin Effects File" },
        { "ape", "Monkey's Audio File" },
        { "api", "Acrobat Plug-in" },
        { "apk", "Android Package" },
        { "apl", "Monkey's Audio Track Information File" },
        { "apm", "ArcPad Map File" },
        { "app", "Application" },
        { "appcache", "HTML5 Cache Manifest File" },
        { "apz", "Autoplay Media Studio Exported Project" },
        { "arc", "Norton Backup Archive" },
        { "arj", "ARJ Compressed File Archive" },
        { "ark", "PowerDesk Pro Archive" },
        { "aro", "SteelArrow Web Application File" },
        { "art", "Artifacts Artifact File" },
        { "as", "ACTIONScript File" },
        { "asd", "Word AutoSave File" },
        { "asf", "Advanced Systems Format File" },
        { "asi", "Alphacam Stone VB Macro File" },
        { "asm", "Assembly Language Source Code File" },
        { "aso", "Assembler Object File" },
        { "asp", "Active Server Page" },
        { "aspx", "Active Server Page Extended File" },
        { "asr", "NEC DDB Application Software File" },
        { "ast", "Clarion for Windows Automated Report" },
        { "asx", "Microsoft ASF Redirector File" },
        { "at2", "EditStudio Title File" },
        { "at3", "Sony ATRAC3 Audio File" },
        { "atf", "Photoshop Transfer Function File" },
        { "atm", "Adobe Type Manager Data File" },
        { "atr", "Atari Disk Image" },
        { "ats", "Advanced ETL Transformation Script" },
        { "att", "Alphacam Lathe Tool File" },
        { "atx", "Adobe Photoshop Texture File" },
        { "au", "Audio File" },
        { "au3", "AutoIt v3 Script" },
        { "aud", "Audacity Audio File" },
        { "aup", "Audacity Project File" },
        { "aut", "AutoIt Script File" },
        { "ava", "AvaaBook eBook" },
        { "avchd", "AVCHD Video" },
        { "avi", "Audio Video Interleave File" },
        { "avi", "AVI Video" },
        { "awb", "Adaptive Multi-Rate Wideband File" },
        { "awe", "Adobe Waveform File" },
        { "awk", "AWK Script" },
        { "ax", "DirectShow Filter" },
        { "axd", "ASP.NET Web Handler File" },
        { "axx", "AxCrypt Encrypted File" },
        { "azw", "Amazon Kindle eBook File" },
        { "azz", "AZZ Cardfile Database File" },
        { "b", "Binary" },
        { "bak", "Backup File" },
        { "bat", "Batch File" },
        { "bgl", "Flight Simulator Scenery File" },
        { "bin", "Binary Data" },
        { "bin", "Binary" },
        { "blend", "Blender Data File" },
        { "bmp", "BMP Image" },
        { "bup", "DVD Backup File" },
        { "c", "C Source Code" },
        { "cab", "Cabinet File" },
        { "cbr", "Comic Book RAR Archive" },
        { "cbz", "Comic Book Zip Archive" },
        { "cdl", "ConceptDraw PRO Library" },
        { "cdr", "CorelDRAW Image File" },
        { "cfg", "Configuration File" },
        { "class", "Java Class" },
        { "coff", "Common Object File Format" },
        { "conf", "Configuration File" },
        { "config", "Configuration File" },
        { "cpp", "C++ Source Code" },
        { "cs", "C# Source Code" },
        { "csh", "C Shell Script" },
        { "csv", "CSV File" },
        { "cue", "Cue Sheet File" },
        { "dat", "Data" },
        { "db", "Database File" },
        { "dcr", "Shockwave Media File" },
        { "dds", "DirectDraw Surface" },
        { "deb", "Debian Package" },
        { "dll", "Dynamic Link Library" },
        { "dmg", "Disk Image" },
        { "dmp", "Dump" },
        { "dng", "Digital Negative Image" },
        { "doc", "Microsoft Word Document" },
        { "docx", "Microsoft Word Document" },
        { "dwg", "AutoCAD Drawing Database File" },
        { "dxf", "Drawing Exchange Format File" },
        { "dylib", "Dynamic Library" },
        { "eml", "Email Message" },
        { "emz", "Compressed Enhanced Metafile" },
        { "epub", "Open eBook File" },
        { "exe", "Executable" },
        { "flac", "Free Lossless Audio Codec File" },
        { "flv", "Flash Video File" },
        { "fon", "Font" },
        { "fsx", "Microsoft Flight Simulator X Save File" },
        { "gadget", "Windows Gadget" },
        { "gif", "GIF Image" },
        { "gitignore", "Git Ignore File" },
        { "gpx", "GPS Exchange File" },
        { "gz", "Gzip Archive" },
        { "h", "C Header" },
        { "hpp", "C++ Header" },
        { "html", "Hypertext Markup Language File" },
        { "ico", "Icon" },
        { "idx", "Index File" },
        { "iff", "Interchange File Format" },
        { "img", "Disk Image" },
        { "indd", "Adobe InDesign Document" },
        { "ini", "INI File" },
        { "iso", "Disk Image" },
        { "jar", "Java Archive File" },
        { "jar", "Java Archive" },
        { "java", "Java Source Code" },
        { "jpe", "JPEG Image" },
        { "jpeg", "JPEG Image" },
        { "jpg", "JPEG Image" },
        { "js", "JavaScript" },
        { "json", "JSON File" },
        { "key", "Keynote Presentation File" },
        { "ko", "Kernel Object" },
        { "lib", "Library" },
        { "lnk", "Shortcut" },
        { "lwo", "LightWave 3D Object File" },
        { "m4a", "MPEG-4 Audio File" },
        { "m4v", "iTunes Video File" },
        { "max", "3ds Max Scene File" },
        { "md", "Markdown Document" },
        { "mkv", "Matroska Video" },
        { "mobi", "Mobipocket eBook" },
        { "mod", "Module" },
        { "mov", "Apple QuickTime Movie" },
        { "mov", "MOV Video" },
        { "mp3", "MP3 Audio" },
        { "mp4", "MP4 Video" },
        { "mpg", "MPEG Video File" },
        { "msi", "Windows Installer Package" },
        { "natvis", "Natvis File" },
        { "nes", "Nintendo (NES) ROM File" },
        { "nfo", "Warez Information File" },
        { "nsf", "NES Sound Format File" },
        { "nzb", "NewzBin Usenet Index File" },
        { "o", "Object" },
        { "obj", "Object" },
        { "ogg", "Ogg Vorbis Audio" },
        { "opus", "Opus Audio" },
        { "orf", "Olympus RAW File" },
        { "p7m", "Digitally Signed Email Message" },
        { "pat", "Pattern File" },
        { "pbp", "PSP Game File" },
        { "pch", "Precompiled Header" },
        { "pcx", "Paintbrush Bitmap Image File" },
        { "pdb", "Program Database" },
        { "pdf", "PDF Document" },
        { "pdf", "Portable Document Format File" },
        { "pem", "Privacy Enhanced Mail Certificate" },
        { "pgm", "PGM Image" },
        { "php", "Hypertext Preprocessor File" },
        { "pkg", "Mac OS X Installer Package" },
        { "pls", "Audio Playlist File" },
        { "png", "PNG Image" },
        { "pnm", "PNM Image" },
        { "pps", "PowerPoint Slide Show" },
        { "ppt", "Microsoft PowerPoint Presentation" },
        { "pptx", "Microsoft PowerPoint Presentation" },
        { "prx", "Plugin" },
        { "ps", "PostScript File" },
        { "psd", "Photoshop Document" },
        { "psp", "PaintShop Pro Image" },
        { "py", "Python Script" },
        { "qbb", "QuickBooks Backup File" },
        { "qbm", "QuickBooks Portable Company File" },
        { "qbw", "QuickBooks Company File" },
        { "qcow2", "QEMU Copy On Write Version 2 Disk Image" },
        { "qfx", "Quicken Financial Exchange File" },
        { "qic", "Windows Backup File" },
        { "ra", "Real Audio File" },
        { "ram", "Real Audio Metadata File" },
        { "rar", "RAR Archive" },
        { "raw", "RAW Image" },
        { "rc", "Resource Script" },
        { "rdata", "R Workspace File" },
        { "res", "Resource" },
        { "rm", "RealMedia File" },
        { "rom", "Read-Only Memory Image" },
        { "rpm", "Red Hat Package Manager File" },
        { "rpm", "RPM Package Manager" },
        { "rtf", "Rich Text Format File" },
        { "sav", "Saved Data" },
        { "sdf", "Standard Data File" },
        { "sh", "Bourne Shell Script" },
        { "so", "Shared Object" },
        { "srt", "SubRip Subtitle File" },
        { "stl", "Stereolithography File" },
        { "svg", "Scalable Vector Graphics File" },
        { "swf", "Shockwave Flash Movie" },
        { "sys", "System File" },
        { "tar.gz", "Compressed Tarball File" },
        { "tar", "Tape Archive" },
        { "tga", "Targa Graphic" },
        { "thm", "Thumbnail Image File" },
        { "tif", "Tagged Image File" },
        { "tiff", "TIFF Image" },
        { "torrent", "BitTorrent File" },
        { "tp", "TurboTax Form File" },
        { "ts", "TypeScript" },
        { "ttf", "TrueType Font File" },
        { "ttf", "TrueType Font" },
        { "txt", "Text File" },
        { "uif", "Universal Image Format File" },
        { "vcf", "vCard File" },
        { "vob", "DVD Video Object File" },
        { "vpk", "Valve Pak" },
        { "vxd", "Virtual Device Driver" },
        { "wav", "WAV Audio" },
        { "wav", "Waveform Audio File" },
        { "wma", "Windows Media Audio File" },
        { "wmf", "Windows Metafile" },
        { "wmv", "WMV Video" },
        { "woff", "Web Open Font Format" },
        { "wpd", "WordPerfect Document" },
        { "wps", "Microsoft Works Word Processor Document" },
        { "xap", "XACT Project File" },
        { "xcf", "GIMP Image File" },
        { "xlr", "Works Spreadsheet" },
        { "xls", "Microsoft Excel Spreadsheet" },
        { "xlsb", "Microsoft Excel Spreadsheet" },
        { "xlsx", "Microsoft Excel Spreadsheet" },
        { "xml", "XML File" },
        { "xpi", "Cross-platform Installer Package" },
        { "xps", "XML Paper Specification File" },
        { "xz", "XZ Archive" },
        { "z", "Z Compressed File" },
        { "z64", "Nintendo 64 ROM File" },
        { "zabw", "Zimbra Address Book File" },
        { "zif", "Zoomable Image Format File" },
        { "zip", "Zip Archive" },
    };

    auto [first_iter, last_iter] = std::equal_range(extension_to_type_text.begin(), extension_to_type_text.end(),
        extension_pair_t(extension, nullptr), [](extension_pair_t const &lhs, extension_pair_t const &rhs) noexcept {
            return strcmp(lhs.first, rhs.first) < 0;
        });

    if (bool found = std::distance(first_iter, last_iter) == 1) {
        return make_str_static<64>(first_iter->second);
    }
    else {
        auto res = make_str_static<64>("%s File", extension);
        std::_Buffer_to_uppercase(res.data(), res.data() + strlen(extension));
        return res;
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
            wchar_t volume_name[MAX_PATH + 1];          cstr_clear(volume_name);
            wchar_t filesystem_name_utf8[MAX_PATH + 1]; cstr_clear(filesystem_name_utf8);
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

generic_result open_file(char const *file_name, char const *file_directory, bool as_admin) noexcept
{
    swan_path target_full_path_utf8 = path_create(file_directory);

    if (!path_append(target_full_path_utf8, file_name, global_state::settings().dir_separator_utf8, true)) {
        print_debug_msg("FAILED path_append, file_directory = [%s], file_name = [\\%s]", file_directory, file_name);
        return { false, "Full file path exceeds max path length." };
    }

    wchar_t target_full_path_utf16[MAX_PATH]; cstr_clear(target_full_path_utf16);

    if (!utf8_to_utf16(target_full_path_utf8.data(), target_full_path_utf16, lengthof(target_full_path_utf16))) {
        return { false, "Conversion of target's full path from UTF-8 to UTF-16." };
    }

    wchar_t const *operation = as_admin ? L"runas" : L"open";

    HINSTANCE result = ShellExecuteW(nullptr, operation, target_full_path_utf16, nullptr, nullptr, SW_SHOWNORMAL);

    auto ec = (intptr_t)result;

    if (ec > HINSTANCE_ERROR) {
        print_debug_msg("SUCCESS ShellExecuteW");
        return { true, target_full_path_utf8.data() };
    }
    else if (ec == SE_ERR_NOASSOC) {
        print_debug_msg("FAILED ShellExecuteW: SE_ERR_NOASSOC");
        return { false, "No association between file type and program (ShellExecuteW: SE_ERR_NOASSOC)." };
    }
    else if (ec == SE_ERR_FNF) {
        print_debug_msg("FAILED ShellExecuteW: SE_ERR_FNF");
        return { false, "File not found (ShellExecuteW: SE_ERR_FNF)." };
    }
    else {
        auto err = get_last_winapi_error().formatted_message;
        print_debug_msg("FAILED ShellExecuteW: %s", err.c_str());
        return { false, err };
    }
}

generic_result open_file_with(char const *file_name, char const *file_directory) noexcept
{
    swan_path target_full_path_utf8 = path_create(file_directory);

    if (!path_append(target_full_path_utf8, file_name, global_state::settings().dir_separator_utf8, true)) {
        print_debug_msg("FAILED path_append, file_directory = [%s], file_name = [\\%s]", file_directory, file_name);
        return { false, "Full file path exceeds max path length." };
    }

    wchar_t target_full_path_utf16[MAX_PATH]; cstr_clear(target_full_path_utf16);

    if (!utf8_to_utf16(target_full_path_utf8.data(), target_full_path_utf16, lengthof(target_full_path_utf16))) {
        return { false, "Conversion of target's full path from UTF-8 to UTF-16." };
    }

    OPENASINFO open_info;
    open_info.pcszClass = NULL;
    open_info.pcszFile = target_full_path_utf16;
    open_info.oaifInFlags = OAIF_EXEC;

    HRESULT result = SHOpenWithDialog(NULL, &open_info);

    if (FAILED(result)) {
        std::string err = _com_error(result).ErrorMessage();
        print_debug_msg("FAILED SHOpenWithDialog: %s", err.c_str());
        return { false, err };
    }
    else {
        print_debug_msg("SUCCESS SHOpenWithDialog");
        return { true, target_full_path_utf8.data() };
    }
}

std::variant<s64, basic_dirent::kind> appropriate_icon(s64 tex_id, basic_dirent::kind obj_type) noexcept
{
    if (global_state::settings().win32_file_icons) return tex_id;
    else return obj_type;
}

void render_path_with_stylish_separators(char const *path, std::variant<s64, basic_dirent::kind> icon) noexcept
{
    swan_path segmented_path = path_create(path);
    assert(!path_is_empty(segmented_path));

    static std::vector<char const *> s_segments = {};
    s_segments.clear();

    {
        char const *segment = strtok(segmented_path.data(), "\\/");
        assert(segment != nullptr);

        while (segment != nullptr) {
            s_segments.push_back(segment);
            segment = strtok(nullptr, "\\/");
        }
    }

    char const *separator = ICON_CI_TRIANGLE_RIGHT;
    imgui::ScopedStyle<f32> s(imgui::GetStyle().ItemSpacing.x, 2);
    imgui::AlignTextToFramePadding();

    for (u64 i = 0; i < s_segments.size() - 1; ++i) {
        char const *segment = s_segments[i];
        imgui::ScopedColor bc(ImGuiCol_Border, imgui::ReduceAlphaTo(imgui::GetStyleColorVec4(ImGuiCol_Text), 0.5f));
        imgui::Button(segment);
        imgui::SameLine();
        imgui::TextDisabled(separator);
        imgui::SameLine();
    }

    if (std::holds_alternative<basic_dirent::kind>(icon)) {
        auto type = std::get<basic_dirent::kind>(icon);
        char const *icon_ = get_icon(type);
        ImVec4 color = get_color(type);
        imgui::AlignTextToFramePadding();
        imgui::TextColored(color, icon_);
    }
    else if (std::holds_alternative<s64>(icon)) {
        f32 icon_size = ImGui::GetFont()->FontSize;
        imgui::SetCursorPosY(imgui::GetCursorPosY() + imgui::GetStyle().FramePadding.y);
        ImGui::Image((ImTextureID)std::max(std::get<s64>(icon), s64(0)), ImVec2(icon_size, icon_size));
    }
    else {
        assert(false);
    }
    imgui::SameLineSpaced(2);
    imgui::AlignTextToFramePadding();
    imgui::TextUnformatted(s_segments.back());
    imgui::SameLineSpaced(1);
}

// static char const *g_help_indicator = "(?)";
static char const *g_help_indicator = ICON_FA_INFO_CIRCLE;

help_indicator render_help_indicator(bool align_text_to_frame_padding) noexcept
{
    help_indicator retval = {};
    if (align_text_to_frame_padding) imgui::AlignTextToFramePadding();
    imgui::TextDisabled(g_help_indicator);
    retval.hovered = imgui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled);
    retval.left_clicked = imgui::IsItemClicked(ImGuiMouseButton_Left);
    retval.right_clicked = imgui::IsItemClicked(ImGuiMouseButton_Right);
    return retval;
}

ImVec2 help_indicator_size() noexcept
{
    return imgui::CalcTextSize(g_help_indicator);
}

ImVec4 compute_drive_usage_color(f32 fraction_used) noexcept
{
    assert(fraction_used >= 0.f);
    assert(fraction_used <= 1.f);

    f32 red = 0;
    f32 green = 1.0f;
    f32 blue = 0;

    if (fraction_used > 0.8f) {
        red = (fraction_used - 0.8f) / 0.2f; // Linearly interpolate red from 0 to 1 as fraction_used goes from 0.8 to 1.0
        green = 1.0f - (fraction_used - 0.8f) / 0.2f; // Linearly interpolate green from 1 to 0 as fraction_used goes from 0.8 to 1.0
    }

    return ImVec4(red, green, blue, 1);
}

std::pair<s64, ImVec2> load_icon_texture(char const *full_path_utf8, wchar_t const *full_path_utf16_provided, char const *debug_label) noexcept
{
    assert((full_path_utf8 || full_path_utf16_provided) && "Provide at least one parameter!");

    if (debug_label) print_debug_msg("load_icon_texture %s", debug_label);
    else print_debug_msg("load_icon_texture");

    wchar_t const *full_path_utf16;

    wchar_t full_path_utf16_buf[MAX_PATH];
    if (full_path_utf16_provided) {
        full_path_utf16 = full_path_utf16_provided;
    } else {
        if (!utf8_to_utf16(full_path_utf8, full_path_utf16_buf, lengthof(full_path_utf16_buf))) {
            return { -1, {} };
        }
        full_path_utf16 = full_path_utf16_buf;
    }

    SHFILEINFOW file_info = {};
    if (!SHGetFileInfoW(full_path_utf16, 0, &file_info, sizeof(file_info), SHGFI_ICON|SHGFI_SMALLICON)) return { -1, {} };
    if (file_info.hIcon == nullptr) return { -1, {} };
    SCOPE_EXIT { if (!DestroyIcon(file_info.hIcon)) print_debug_msg("FAILED DestroyIcon"); };

    ICONINFO icon_info = {};
    if (!GetIconInfo(file_info.hIcon, &icon_info)) return { -1, {} };
    if (icon_info.hbmColor == nullptr) return { -1, {} };
    SCOPE_EXIT { if (!DeleteObject(icon_info.hbmColor)) print_debug_msg("FAILED DeleteObject(hbmColor)");
                 if (!DeleteObject(icon_info.hbmMask)) print_debug_msg("FAILED DeleteObject(hbmMask)"); };

    DIBSECTION ds;
    if (!GetObjectA(icon_info.hbmColor, sizeof(ds), &ds)) return { -1, {} };

    u64 num_bytes_pixels = ds.dsBm.bmWidth * ds.dsBm.bmHeight * (ds.dsBm.bmBitsPixel / 8);
    if (num_bytes_pixels == 0) return { -1, {} };

    auto pixel_data = std::make_unique<u8[]>(num_bytes_pixels);
    if (!GetBitmapBits(icon_info.hbmColor, (LONG)num_bytes_pixels, pixel_data.get())) return { -1, {} };

    char fmt = 0;
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ds.dsBm.bmWidth, ds.dsBm.bmHeight, 0, (fmt == 0) ? GL_BGRA : GL_RGBA, GL_UNSIGNED_BYTE, pixel_data.get());
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);

    return { tex, { f32(ds.dsBm.bmWidth), f32(ds.dsBm.bmHeight) } };
}

void delete_icon_texture(s64 &id, char const *debug_label) noexcept
{
    assert(id > 0 && "Don't call on non-existent texture!");
    if (debug_label) print_debug_msg("delete_icon_texture %s", debug_label);
    else print_debug_msg("delete_icon_texture");
    GLuint gl_id = static_cast<GLuint>(id);
    glDeleteTextures(1, &gl_id);
    id = 0;
}

void open_file_properties(char const *full_path_utf8) noexcept
{
    wchar_t full_path_utf16[MAX_PATH];

    if (!utf8_to_utf16(full_path_utf8, full_path_utf16, sizeof(full_path_utf16))) {
        return;
    }

    SHELLEXECUTEINFOW shExecInfo = {0};
    shExecInfo.cbSize = sizeof(shExecInfo);
    shExecInfo.fMask = SEE_MASK_INVOKEIDLIST;
    shExecInfo.hwnd = NULL;
    shExecInfo.lpVerb = L"properties";
    shExecInfo.lpFile = full_path_utf16;
    shExecInfo.lpParameters = L"";
    shExecInfo.lpDirectory = NULL;
    shExecInfo.nShow = SW_SHOW;
    shExecInfo.hInstApp = NULL;

    ShellExecuteExW(&shExecInfo);
}
