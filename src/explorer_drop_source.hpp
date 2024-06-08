#pragma once

#include "stdafx.hpp"

struct explorer_drop_source final
    : public IDropSource // https://learn.microsoft.com/en-us/windows/win32/api/oleidl/nn-oleidl-idropsource
    , public IDataObject // https://learn.microsoft.com/en-us/windows/win32/api/objidl/nn-objidl-idataobject
{

    ULONG AddRef() noexcept override;

    ULONG Release() noexcept override;

    HRESULT QueryInterface(REFIID riid, void **ppvObject) noexcept override;

    HRESULT QueryContinueDrag(BOOL escape_pressed, DWORD key_state) noexcept override;

    HRESULT GiveFeedback(DWORD effect) noexcept override;

    HRESULT GetData(FORMATETC *format_etc, STGMEDIUM *medium) noexcept override;

    HRESULT GetDataHere(FORMATETC *pformatetc, STGMEDIUM *pmedium) noexcept override;

    HRESULT QueryGetData(FORMATETC *pformatetc) noexcept override;

    HRESULT GetCanonicalFormatEtc(FORMATETC *pformatectIn, FORMATETC *pformatetcOut) noexcept override;

    HRESULT SetData(FORMATETC *pformatetc, STGMEDIUM *pmedium, BOOL fRelease) noexcept override;

    HRESULT EnumFormatEtc(DWORD dwDirection, IEnumFORMATETC **ppenumFormatEtc) noexcept override;

    HRESULT DAdvise(FORMATETC *pformatetc, DWORD advf, IAdviseSink *pAdvSink, DWORD *pdwConnection) noexcept override;

    HRESULT DUnadvise(DWORD dwConnection) noexcept override;

    HRESULT EnumDAdvise(IEnumSTATDATA **ppenumAdvise) noexcept override;

    std::wstring full_paths_delimited_by_newlines = {};

private:

    volatile long ref_count = 1;

    bool supported_FORMATETC(FORMATETC const *format_etc) noexcept;
};
