#include "stdafx.hpp"
#include "explorer_drop_source.hpp"
#include "imgui_dependent_functions.hpp"
#include "imgui_extension.hpp"
#include "util.hpp"

std::pair<IID, char const *> const g_IIDs[] = {
    { IID_IMarshal, "IID_IMarshal" },
    { IID_INoMarshal, "IID_INoMarshal" },
    { IID_IAgileObject, "IID_IAgileObject" },
    { IID_IActivationFilter, "IID_IActivationFilter" },
    { IID_IMarshal2, "IID_IMarshal2" },
    { IID_IMalloc, "IID_IMalloc" },
    { IID_IStdMarshalInfo, "IID_IStdMarshalInfo" },
    { IID_IExternalConnection, "IID_IExternalConnection" },
    { IID_IMultiQI, "IID_IMultiQI" },
    { IID_AsyncIMultiQI, "IID_AsyncIMultiQI" },
    { IID_IInternalUnknown, "IID_IInternalUnknown" },
    { IID_IEnumUnknown, "IID_IEnumUnknown" },
    { IID_IEnumString, "IID_IEnumString" },
    { IID_ISequentialStream, "IID_ISequentialStream" },
    { IID_IStream, "IID_IStream" },
    { IID_IRpcChannelBuffer, "IID_IRpcChannelBuffer" },
    { IID_IRpcChannelBuffer2, "IID_IRpcChannelBuffer2" },
    { IID_IAsyncRpcChannelBuffer, "IID_IAsyncRpcChannelBuffer" },
    { IID_IRpcChannelBuffer3, "IID_IRpcChannelBuffer3" },
    { IID_IRpcSyntaxNegotiate, "IID_IRpcSyntaxNegotiate" },
    { IID_IRpcProxyBuffer, "IID_IRpcProxyBuffer" },
    { IID_IRpcStubBuffer, "IID_IRpcStubBuffer" },
    { IID_IPSFactoryBuffer, "IID_IPSFactoryBuffer" },
    { IID_IChannelHook, "IID_IChannelHook" },
    { IID_IClientSecurity, "IID_IClientSecurity" },
    { IID_IServerSecurity, "IID_IServerSecurity" },
    { IID_IRpcOptions, "IID_IRpcOptions" },
    { IID_IGlobalOptions, "IID_IGlobalOptions" },
    { IID_ISurrogate, "IID_ISurrogate" },
    { IID_IGlobalInterfaceTable, "IID_IGlobalInterfaceTable" },
    { IID_ISynchronize, "IID_ISynchronize" },
    { IID_ISynchronizeHandle, "IID_ISynchronizeHandle" },
    { IID_ISynchronizeEvent, "IID_ISynchronizeEvent" },
    { IID_ISynchronizeContainer, "IID_ISynchronizeContainer" },
    { IID_ISynchronizeMutex, "IID_ISynchronizeMutex" },
    { IID_ICancelMethodCalls, "IID_ICancelMethodCalls" },
    { IID_IAsyncManager, "IID_IAsyncManager" },
    { IID_ICallFactory, "IID_ICallFactory" },
    { IID_IRpcHelper, "IID_IRpcHelper" },
    { IID_IReleaseMarshalBuffers, "IID_IReleaseMarshalBuffers" },
    { IID_IWaitMultiple, "IID_IWaitMultiple" },
    { IID_IAddrTrackingControl, "IID_IAddrTrackingControl" },
    { IID_IAddrExclusionControl, "IID_IAddrExclusionControl" },
    { IID_IPipeByte, "IID_IPipeByte" },
    { IID_AsyncIPipeByte, "IID_AsyncIPipeByte" },
    { IID_IPipeLong, "IID_IPipeLong" },
    { IID_AsyncIPipeLong, "IID_AsyncIPipeLong" },
    { IID_IPipeDouble, "IID_IPipeDouble" },
    { IID_AsyncIPipeDouble, "IID_AsyncIPipeDouble" },
    // { IID_IEnumContextProps, "IID_IEnumContextProps" },
    // { IID_IContext, "IID_IContext" },
    // { IID_IObjContext, "IID_IObjContext" },
    { IID_IComThreadingInfo, "IID_IComThreadingInfo" },
    { IID_IProcessInitControl, "IID_IProcessInitControl" },
    { IID_IFastRundown, "IID_IFastRundown" },
    { IID_IMarshalingStream, "IID_IMarshalingStream" },
    { IID_IAgileReference, "IID_IAgileReference" },
    { IID_IAgileReference, "IID_IAgileReference" },
    { IID_IMallocSpy, "IID_IMallocSpy" },
    { IID_IBindCtx, "IID_IBindCtx" },
    { IID_IEnumMoniker, "IID_IEnumMoniker" },
    { IID_IRunnableObject, "IID_IRunnableObject" },
    { IID_IRunningObjectTable, "IID_IRunningObjectTable" },
    { IID_IPersist, "IID_IPersist" },
    { IID_IPersistStream, "IID_IPersistStream" },
    { IID_IMoniker, "IID_IMoniker" },
    { IID_IROTData, "IID_IROTData" },
    { IID_IEnumSTATSTG, "IID_IEnumSTATSTG" },
    { IID_IStorage, "IID_IStorage" },
    { IID_IPersistFile, "IID_IPersistFile" },
    { IID_IPersistStorage, "IID_IPersistStorage" },
    { IID_ILockBytes, "IID_ILockBytes" },
    { IID_IEnumFORMATETC, "IID_IEnumFORMATETC" },
    { IID_IEnumSTATDATA, "IID_IEnumSTATDATA" },
    { IID_IRootStorage, "IID_IRootStorage" },
    { IID_IAdviseSink, "IID_IAdviseSink" },
    { IID_AsyncIAdviseSink, "IID_AsyncIAdviseSink" },
    { IID_IAdviseSink2, "IID_IAdviseSink2" },
    { IID_AsyncIAdviseSink2, "IID_AsyncIAdviseSink2" },
    { IID_IDataObject, "IID_IDataObject" },
    { IID_IDataAdviseHolder, "IID_IDataAdviseHolder" },
    { IID_IMessageFilter, "IID_IMessageFilter" },
    { IID_IClassActivator, "IID_IClassActivator" },
    { IID_IFillLockBytes, "IID_IFillLockBytes" },
    { IID_IProgressNotify, "IID_IProgressNotify" },
    { IID_ILayoutStorage, "IID_ILayoutStorage" },
    { IID_IBlockingLock, "IID_IBlockingLock" },
    { IID_ITimeAndNoticeControl, "IID_ITimeAndNoticeControl" },
    { IID_IOplockStorage, "IID_IOplockStorage" },
    { IID_IDirectWriterLock, "IID_IDirectWriterLock" },
    { IID_IUrlMon, "IID_IUrlMon" },
    { IID_IForegroundTransfer, "IID_IForegroundTransfer" },
    { IID_IThumbnailExtractor, "IID_IThumbnailExtractor" },
    { IID_IDummyHICONIncluder, "IID_IDummyHICONIncluder" },
    { IID_IProcessLock, "IID_IProcessLock" },
    { IID_ISurrogateService, "IID_ISurrogateService" },
    { IID_IInitializeSpy, "IID_IInitializeSpy" },
    { IID_IApartmentShutdown, "IID_IApartmentShutdown" },
};

ULONG explorer_drop_source::AddRef() noexcept
{
    print_debug_msg("explorer_drop_source::AddRef");
    return InterlockedIncrement(&this->ref_count);
}

ULONG explorer_drop_source::Release() noexcept
{
    print_debug_msg("explorer_drop_source::Release");
    ULONG count = InterlockedDecrement(&this->ref_count);
    if (count == 0) {
        delete this;
        full_paths_delimited_by_newlines.~full_paths_delimited_by_newlines();
    }
    return count;
}

HRESULT explorer_drop_source::QueryInterface(REFIID riid, void** ppvObject) noexcept
{
    if (!ppvObject) {
        return E_POINTER;
    }

    // Print REFIID
    for (auto const &[iid, name] : g_IIDs) {
        if (riid == iid) {
            wchar_t *iid_utf16 = nullptr;
            if (StringFromIID(riid, &iid_utf16) == S_OK) {
                std::array<char, 128> iid_utf8;
                assert(utf16_to_utf8(iid_utf16, iid_utf8.data(), iid_utf8.max_size()));
                print_debug_msg("explorer_drop_source::QueryInterface(riid = %s)", iid_utf8.data());
            }
            break;
        }
    }

    if (riid == IID_IUnknown) {
        *ppvObject = static_cast<IUnknown*>(static_cast<IDropSource*>(this));
    } else if (riid == IID_IDropSource) {
        *ppvObject = static_cast<IDropSource*>(this);
    } else if (riid == IID_IDataObject) {
        *ppvObject = static_cast<IDataObject*>(this);
    } else {
        *ppvObject = nullptr;
        return E_NOINTERFACE;
    }

    AddRef();
    return S_OK;
}

HRESULT explorer_drop_source::QueryContinueDrag(BOOL escape_pressed, DWORD key_state) noexcept
{
    // print_debug_msg("explorer_drop_source::QueryContinueDrag");

    if (escape_pressed) {
        print_debug_msg("QueryContinueDrag: Cancel, user pressed escape");
        return DRAGDROP_S_CANCEL;
    }
    if ((key_state & MK_LBUTTON) == false) {
        print_debug_msg("QueryContinueDrag: Drop, user released mouse 1");
        return DRAGDROP_S_DROP;
    }

    std::optional<bool> mouse_inside_window = win32_is_mouse_inside_window(global_state::window_handle());
    if (mouse_inside_window.has_value() && mouse_inside_window.value() == true) {
        print_debug_msg("QueryContinueDrag: Cancel, mouse cursor inside main window");
        return DRAGDROP_S_CANCEL;
    }

    return S_OK;
}

HRESULT explorer_drop_source::GiveFeedback(DWORD effect) noexcept
{
    // print_debug_msg("explorer_drop_source::GiveFeedback");

    if (effect == DROPEFFECT_NONE) {
        // print_debug_msg("DROPEFFECT_NONE, IDropTarget::DragLeave");
    }
    else {

    }

    return DRAGDROP_S_USEDEFAULTCURSORS;
    // return S_OK;
}

// This class was generated by ChatGPT 3.5 ... I don't have time to write this much stupid code.
class EnumFormatEtcImpl : public IEnumFORMATETC
{
private:
    ULONG mRefCount;
    ULONG mIndex;
    ULONG mNumFormats;
    FORMATETC* mFormats;

public:
    EnumFormatEtcImpl(FORMATETC* formats, ULONG numFormats) noexcept
        : mRefCount(1), mIndex(0), mNumFormats(numFormats)
    {
        print_debug_msg("EnumFormatEtcImpl::EnumFormatEtcImpl");
        mFormats = new FORMATETC[numFormats];
        memcpy(mFormats, formats, sizeof(FORMATETC) * numFormats);
    }

    ~EnumFormatEtcImpl() noexcept
    {
        print_debug_msg("EnumFormatEtcImpl::~EnumFormatEtcImpl");
        delete[] mFormats;
    }

    // IUnknown methods
    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObject) noexcept
    {
        print_debug_msg("EnumFormatEtcImpl::QueryInterface");

        if (ppvObject == nullptr)
            return E_INVALIDARG;

        *ppvObject = nullptr;

        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IEnumFORMATETC))
        {
            *ppvObject = static_cast<IEnumFORMATETC*>(this);
            AddRef();
            return S_OK;
        }

        return E_NOINTERFACE;
    }

    STDMETHODIMP_(ULONG) AddRef() noexcept
    {
        print_debug_msg("EnumFormatEtcImpl::AddRef");
        return InterlockedIncrement(&mRefCount);
    }

    STDMETHODIMP_(ULONG) Release() noexcept
    {
        print_debug_msg("EnumFormatEtcImpl::Release");
        ULONG newRefCount = InterlockedDecrement(&mRefCount);
        if (newRefCount == 0) {
            delete this;
        }
        return newRefCount;
    }

    STDMETHODIMP Next(ULONG celt, FORMATETC* rgelt, ULONG* pceltFetched) noexcept
    {
        print_debug_msg("EnumFormatEtcImpl::Next");

        if (pceltFetched != nullptr)
            *pceltFetched = 0;

        if (rgelt == nullptr || (celt > 1 && pceltFetched == nullptr))
            return E_INVALIDARG;

        ULONG fetched = 0;

        while (mIndex < mNumFormats && fetched < celt)
        {
            rgelt[fetched] = mFormats[mIndex];
            ++mIndex;
            ++fetched;
        }

        if (pceltFetched != nullptr)
            *pceltFetched = fetched;

        return (fetched == celt) ? S_OK : S_FALSE;
    }

    STDMETHODIMP Skip(ULONG celt) noexcept
    {
        print_debug_msg("EnumFormatEtcImpl::Skip");
        mIndex += celt;
        return (mIndex <= mNumFormats) ? S_OK : S_FALSE;
    }

    STDMETHODIMP Reset() noexcept
    {
        print_debug_msg("EnumFormatEtcImpl::Reset");
        mIndex = 0;
        return S_OK;
    }

    STDMETHODIMP Clone(IEnumFORMATETC** ppEnum) noexcept
    {
        print_debug_msg("EnumFormatEtcImpl::Clone");

        if (ppEnum == nullptr)
            return E_INVALIDARG;

        *ppEnum = new EnumFormatEtcImpl(mFormats, mNumFormats);
        return (*ppEnum != nullptr) ? S_OK : E_OUTOFMEMORY;
    }
};

#if 0
bool explorer_drop_source::supported_FORMATETC(FORMATETC const *format_etc) noexcept
{
        std::pair<s32, char const *> const clipboard_formats[] = {
        { CF_TEXT, "TEXT" },
        { CF_BITMAP, "BITMAP" },
        { CF_METAFILEPICT, "METAFILEPICT" },
        { CF_SYLK, "SYLK" },
        { CF_DIF, "DIF" },
        { CF_TIFF, "TIFF" },
        { CF_OEMTEXT, "OEMTEXT" },
        { CF_DIB, "DIB" },
        { CF_PALETTE, "PALETTE" },
        { CF_PENDATA, "PENDATA" },
        { CF_RIFF, "RIFF" },
        { CF_WAVE, "WAVE" },
        { CF_UNICODETEXT, "UNICODETEXT" },
        { CF_ENHMETAFILE, "ENHMETAFILE" },
        { CF_HDROP, "HDROP" },
        { CF_LOCALE, "LOCALE" },
        { CF_DIBV5, "DIBV5" },
        { CF_MAX, "MAX" },
        { CF_OWNERDISPLAY, "OWNERDISPLAY" },
        { CF_DSPTEXT, "DSPTEXT" },
        { CF_DSPBITMAP, "DSPBITMAP" },
        { CF_DSPMETAFILEPICT, "DSPMETAFILEPICT" },
        { CF_DSPENHMETAFILE, "DSPENHMETAFILE" },
        { CF_PRIVATEFIRST, "PRIVATEFIRST" },
        { CF_PRIVATELAST, "PRIVATELAST" },
        { CF_GDIOBJFIRST, "GDIOBJFIRST" },
        { CF_GDIOBJLAST, "GDIOBJLAST" },
    };

    std::pair<TYMED, char const *> const medium_types[] = {
        { TYMED_HGLOBAL, "HGLOBAL" },
        { TYMED_FILE, "FILE" },
        { TYMED_ISTREAM, "ISTREAM" },
        { TYMED_ISTORAGE, "ISTORAGE" },
        { TYMED_GDI, "GDI" },
        { TYMED_MFPICT, "MFPICT" },
        { TYMED_ENHMF, "ENHMF" },
        { TYMED_NULL, "NULL" },
    };

    char const *clipboard_format_name = "Unknown";
    char const *medium_type_name = "Unknown";

    for (auto const &[cf, name] : clipboard_formats) {
        if (format_etc->cfFormat == cf) {
            clipboard_format_name = name;
            break;
        }
    }

    for (auto const &[tymed, name] : medium_types) {
        if (format_etc->tymed == tymed) {
            medium_type_name = name;
            break;
        }
    }

    bool supported = format_etc->cfFormat == CF_HDROP && format_etc->tymed == TYMED_HGLOBAL;
    print_debug_msg("FORMATETC supported [%d] cfFormat (%d, %s) tymed (%d, %s)",
                    supported, format_etc->cfFormat, clipboard_format_name, format_etc->tymed, medium_type_name);

    return supported;
}
#else
bool explorer_drop_source::supported_FORMATETC(FORMATETC const *format_etc) noexcept
{
    return (format_etc->cfFormat == CF_HDROP &&
            format_etc->dwAspect == DVASPECT_CONTENT &&
            format_etc->tymed == TYMED_HGLOBAL);
}
#endif

HRESULT explorer_drop_source::GetData(FORMATETC *format_etc, STGMEDIUM *medium) noexcept
try {
    if (!supported_FORMATETC(format_etc)) {
        return DV_E_FORMATETC;
    }

    if (format_etc->cfFormat == CF_HDROP) {
        auto &paths = full_paths_delimited_by_newlines;
        size_t total_payload_size = sizeof(DROPFILES) + (sizeof(wchar_t) * (paths.size() + 1));

        // Allocate global memory for the DROPFILES structure
        HGLOBAL global_mem_handle = GlobalAlloc(GHND, total_payload_size);
        if (!global_mem_handle) {
            return E_OUTOFMEMORY;
        }

        // Lock the global memory and fill it with the DROPFILES structure and file paths
        {
            DROPFILES *drop_files = (DROPFILES *) GlobalLock(global_mem_handle);
            if (!drop_files) {
                GlobalFree(global_mem_handle);
                return E_OUTOFMEMORY;
            }
            SCOPE_EXIT { GlobalUnlock(global_mem_handle); };

            drop_files->pFiles = sizeof(DROPFILES);
            drop_files->pt.x = 0;
            drop_files->pt.y = 0;
            drop_files->fNC = TRUE;
            drop_files->fWide = TRUE;

            wchar_t *memory_after_DROPFILES = (wchar_t *)((BYTE *)drop_files + sizeof(DROPFILES));
            std::transform(paths.begin(), paths.end(), memory_after_DROPFILES, [](wchar_t ch) noexcept { return ch == L'\n' ? L'\0' : ch; });
        }

        medium->tymed = TYMED_HGLOBAL;
        medium->hGlobal = global_mem_handle;
        medium->pUnkForRelease = nullptr;

        return S_OK;
    }

    return DV_E_FORMATETC;
}
catch (...) {
    return E_UNEXPECTED;
}

/// Docs: https://learn.microsoft.com/en-us/windows/win32/api/objidl/nf-objidl-idataobject-getdatahere
HRESULT explorer_drop_source::GetDataHere(FORMATETC */*format_etc*/, STGMEDIUM */*medium*/) noexcept
{
    print_debug_msg("explorer_drop_source::GetDataHere");
    return E_NOTIMPL;
}

/// Docs: https://learn.microsoft.com/en-us/windows/win32/api/objidl/nf-objidl-idataobject-setdata
HRESULT explorer_drop_source::SetData(FORMATETC */*format_etc*/, STGMEDIUM */*medium*/, BOOL /*release*/) noexcept
{
    print_debug_msg("explorer_drop_source::SetData");
    return E_NOTIMPL;
}

/// A function which the target application (say iTunes) might call to check if we support a FORMATETC.
/// Docs: https://learn.microsoft.com/en-us/windows/win32/api/objidl/nf-objidl-idataobject-querygetdata
HRESULT explorer_drop_source::QueryGetData(FORMATETC *format_etc) noexcept
{
    print_debug_msg("explorer_drop_source::QueryGetData");

    if (!supported_FORMATETC(format_etc)) {
        return DV_E_FORMATETC;
    }
    return S_OK;
}

/// A function for the target application (say iTunes) to check logical equivalence of two FORMATETC objects.
/// Docs: https://learn.microsoft.com/en-us/windows/win32/api/objidl/nf-objidl-idataobject-getcanonicalformatetc
HRESULT explorer_drop_source::GetCanonicalFormatEtc(FORMATETC *format_etc_in, FORMATETC *format_etc_out) noexcept
{
    print_debug_msg("explorer_drop_source::GetCanonicalFormatEtc");

    if (format_etc_in->cfFormat != CF_HDROP) {
        format_etc_out->cfFormat = CF_HDROP;
        return S_OK; // The returned FORMATETC structure is different from the one that was passed.
    } else {
        format_etc_out->ptd = nullptr;
        return DATA_S_SAMEFORMATETC; // The FORMATETC structures are the same and NULL is returned in pformatetcOut.
    }
}

/// A way for the target application (say iTunes) to find out what FORMATETC's our app supports.
/// We only support one: CF_HDROP via TYMED_HGLOBAL.
/// Docs: https://learn.microsoft.com/en-us/windows/win32/api/objidl/nf-objidl-idataobject-enumformatetc
HRESULT explorer_drop_source::EnumFormatEtc(DWORD direction, IEnumFORMATETC **enum_format_etc_out) noexcept
{
    print_debug_msg("explorer_drop_source::EnumFormatEtc");

    if (enum_format_etc_out == nullptr) {
        print_debug_msg("ERROR: enum_format_etc_out == nullptr");
        return E_INVALIDARG;
    }

    *enum_format_etc_out = nullptr;

    if (direction == DATADIR_GET) {
        // Allocate and initialize a FORMATETC structure for CF_HDROP
        FORMATETC format_etc;
        format_etc.cfFormat = CF_HDROP;
        format_etc.ptd = nullptr;
        format_etc.dwAspect = DVASPECT_CONTENT;
        format_etc.lindex = -1;
        format_etc.tymed = TYMED_HGLOBAL;

        // Create an enumeration object that contains a single format
        *enum_format_etc_out = new EnumFormatEtcImpl(&format_etc, 1);

        if (*enum_format_etc_out == nullptr) {
            return E_OUTOFMEMORY;
        }

        print_debug_msg("direction == DATADIR_GET, returning S_OK");
        return S_OK;
    }

    print_debug_msg("direction != DATADIR_GET, returning E_NOTIMPL");
    return E_NOTIMPL;
}

// https://learn.microsoft.com/en-us/windows/win32/api/objidl/nf-objidl-idataobject-dadvise
HRESULT explorer_drop_source::DAdvise(FORMATETC */*format_etc*/, DWORD /*advf*/, IAdviseSink */*advise_sink*/, DWORD */*connection*/) noexcept
{
    print_debug_msg("explorer_drop_source::DAdvise -> OLE_E_ADVISENOTSUPPORTED");
    return OLE_E_ADVISENOTSUPPORTED;
}

// https://learn.microsoft.com/en-us/windows/win32/api/objidl/nf-objidl-idataobject-dunadvise
HRESULT explorer_drop_source::DUnadvise(DWORD /*connection*/) noexcept
{
    print_debug_msg("explorer_drop_source::DUnadvise -> OLE_E_ADVISENOTSUPPORTED");
    return OLE_E_ADVISENOTSUPPORTED;
}

// https://learn.microsoft.com/en-us/windows/win32/api/objidl/nf-objidl-idataobject-enumdadvise
HRESULT explorer_drop_source::EnumDAdvise(IEnumSTATDATA **/*enum_advise*/) noexcept
{
    print_debug_msg("explorer_drop_source::EnumDAdvise -> OLE_E_ADVISENOTSUPPORTED");
    return OLE_E_ADVISENOTSUPPORTED;
}
