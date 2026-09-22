#include "VrcwDropTarget.h"

#include "PathUtils.h"

#include <ole2.h>
#include <shellapi.h>

#include <string>
#include <vector>

namespace
{
    //
    // Reads the dropped file paths out of a data object.
    //
    std::vector<std::wstring> GetDroppedPaths(
        IDataObject* dataObject)
    {
        std::vector<std::wstring> paths;

        if (dataObject == nullptr)
        {
            return paths;
        }

        FORMATETC format{};

        format.cfFormat = CF_HDROP;
        format.dwAspect = DVASPECT_CONTENT;
        format.lindex = -1;
        format.tymed = TYMED_HGLOBAL;

        STGMEDIUM medium{};

        if (FAILED(dataObject->GetData(
            &format,
            &medium)))
        {
            return paths;
        }

        HDROP drop =
            static_cast<HDROP>(
                GlobalLock(medium.hGlobal));

        if (drop != nullptr)
        {
            const UINT fileCount =
                DragQueryFileW(
                    drop,
                    0xFFFFFFFFu,
                    nullptr,
                    0);

            for (UINT index = 0;
                index < fileCount;
                ++index)
            {
                const UINT length =
                    DragQueryFileW(
                        drop,
                        index,
                        nullptr,
                        0);

                if (length == 0)
                {
                    continue;
                }

                std::wstring path(
                    static_cast<size_t>(length) + 1,
                    L'\0');

                DragQueryFileW(
                    drop,
                    index,
                    path.data(),
                    length + 1);

                path.resize(length);

                paths.push_back(path);
            }

            GlobalUnlock(medium.hGlobal);
        }

        ReleaseStgMedium(&medium);

        return paths;
    }

    std::wstring GetFirstVrcwPath(
        IDataObject* dataObject)
    {
        for (const std::wstring& path :
            GetDroppedPaths(dataObject))
        {
            if (PathUtils::IsVrcwFile(path) &&
                PathUtils::FileExists(path))
            {
                return path;
            }
        }

        return std::wstring();
    }

    class VrcwDropTargetImpl final : public IDropTarget
    {
    public:
        VrcwDropTargetImpl(
            HWND window,
            VrcwDropTarget::PathCallback onDrop,
            VrcwDropTarget::HoverCallback onHover)
            : m_refCount(1),
            m_window(window),
            m_onDrop(onDrop),
            m_onHover(onHover),
            m_accepts(false),
            m_hovering(false)
        {
        }

        HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID interfaceId,
            void** object) override
        {
            if (object == nullptr)
            {
                return E_POINTER;
            }

            if (interfaceId == IID_IUnknown ||
                interfaceId == IID_IDropTarget)
            {
                *object =
                    static_cast<IDropTarget*>(this);

                AddRef();

                return S_OK;
            }

            *object = nullptr;

            return E_NOINTERFACE;
        }

        ULONG STDMETHODCALLTYPE AddRef() override
        {
            return static_cast<ULONG>(
                InterlockedIncrement(&m_refCount));
        }

        ULONG STDMETHODCALLTYPE Release() override
        {
            const ULONG count =
                static_cast<ULONG>(
                    InterlockedDecrement(&m_refCount));

            if (count == 0)
            {
                delete this;
            }

            return count;
        }

        HRESULT STDMETHODCALLTYPE DragEnter(
            IDataObject* dataObject,
            DWORD,
            POINTL,
            DWORD* effect) override
        {
            m_accepts =
                !GetFirstVrcwPath(dataObject).empty();

            UpdateHover(m_accepts);

            if (effect != nullptr)
            {
                *effect = m_accepts
                    ? DROPEFFECT_COPY
                    : DROPEFFECT_NONE;
            }

            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE DragOver(
            DWORD,
            POINTL,
            DWORD* effect) override
        {
            if (effect != nullptr)
            {
                *effect = m_accepts
                    ? DROPEFFECT_COPY
                    : DROPEFFECT_NONE;
            }

            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE DragLeave() override
        {
            m_accepts = false;

            UpdateHover(false);

            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE Drop(
            IDataObject* dataObject,
            DWORD,
            POINTL,
            DWORD* effect) override
        {
            const std::wstring path =
                GetFirstVrcwPath(dataObject);

            m_accepts = false;

            UpdateHover(false);

            if (effect != nullptr)
            {
                *effect = path.empty()
                    ? DROPEFFECT_NONE
                    : DROPEFFECT_COPY;
            }

            if (!path.empty() && m_onDrop != nullptr)
            {
                m_onDrop(path);
            }

            return S_OK;
        }

    private:
        void UpdateHover(
            bool hovering)
        {
            if (m_hovering == hovering)
            {
                return;
            }

            m_hovering = hovering;

            if (m_onHover != nullptr)
            {
                m_onHover(hovering);
            }
        }

        LONG m_refCount;
        HWND m_window;
        VrcwDropTarget::PathCallback m_onDrop;
        VrcwDropTarget::HoverCallback m_onHover;
        bool m_accepts;
        bool m_hovering;
    };

    VrcwDropTargetImpl* g_dropTarget = nullptr;
}

namespace VrcwDropTarget
{
    bool Register(
        HWND window,
        PathCallback onDrop,
        HoverCallback onHover)
    {
        if (window == nullptr)
        {
            return false;
        }

        Unregister(window);

        g_dropTarget =
            new VrcwDropTargetImpl(
                window,
                onDrop,
                onHover);

        const HRESULT result =
            RegisterDragDrop(
                window,
                g_dropTarget);

        if (FAILED(result))
        {
            g_dropTarget->Release();
            g_dropTarget = nullptr;

            return false;
        }

        return true;
    }

    void Unregister(
        HWND window)
    {
        if (g_dropTarget == nullptr)
        {
            return;
        }

        if (window != nullptr)
        {
            RevokeDragDrop(window);
        }

        g_dropTarget->Release();
        g_dropTarget = nullptr;
    }
}
