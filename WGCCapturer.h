#pragma once
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Metadata.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>

#include <windows.h>
#include <mutex>
#include <atomic>
#include <opencv2/opencv.hpp>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <dwmapi.h>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "windowsapp.lib")

class WGCCapturer {
private:
    HWND m_targetHwnd = nullptr;
    winrt::com_ptr<ID3D11Device> m_d3dDevice;
    winrt::com_ptr<ID3D11DeviceContext> m_d3dContext;
    winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice m_device{ nullptr };
    winrt::Windows::Graphics::Capture::GraphicsCaptureItem m_item{ nullptr };
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool m_framePool{ nullptr };
    winrt::Windows::Graphics::Capture::GraphicsCaptureSession m_session{ nullptr };
    winrt::com_ptr<ID3D11Texture2D> m_stagingTexture;

    std::mutex m_frameMutex;
    cv::Mat m_latestMat;
    std::atomic<bool> m_isCapturing{ false };

    // 偏移校准参数
    int m_offsetX = 0; // 左边框宽度 (用于补齐X轴)
    int m_offsetY = 0; // 标题栏高度 (用于偏移Y轴)

    void OnFrameArrived(winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool const& sender, winrt::Windows::Foundation::IInspectable const&) {
        auto frame = sender.TryGetNextFrame();
        if (!frame) return;

        try {
            auto access = frame.Surface().as<Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();
            winrt::com_ptr<ID3D11Texture2D> frameTexture;
            winrt::check_hresult(access->GetInterface(winrt::guid_of<ID3D11Texture2D>(), frameTexture.put_void()));

            D3D11_TEXTURE2D_DESC desc;
            frameTexture->GetDesc(&desc);

            std::lock_guard<std::mutex> lock(m_frameMutex);

            if (m_stagingTexture) {
                D3D11_TEXTURE2D_DESC sDesc;
                m_stagingTexture->GetDesc(&sDesc);
                if (sDesc.Width != desc.Width || sDesc.Height != desc.Height) {
                    m_stagingTexture = nullptr;
                }
            }

            if (!m_stagingTexture) {
                D3D11_TEXTURE2D_DESC stagingDesc = desc;
                stagingDesc.Usage = D3D11_USAGE_STAGING;
                stagingDesc.BindFlags = 0;
                stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
                stagingDesc.MiscFlags = 0;
                m_d3dDevice->CreateTexture2D(&stagingDesc, nullptr, m_stagingTexture.put());
            }

            if (m_stagingTexture && m_d3dContext) {
                m_d3dContext->CopyResource(m_stagingTexture.get(), frameTexture.get());
                D3D11_MAPPED_SUBRESOURCE mapped;
                if (SUCCEEDED(m_d3dContext->Map(m_stagingTexture.get(), 0, D3D11_MAP_READ, 0, &mapped))) {
                    cv::Mat tempMat(desc.Height, desc.Width, CV_8UC4, mapped.pData, mapped.RowPitch);
                    if (!tempMat.empty()) {
                        cv::cvtColor(tempMat, m_latestMat, cv::COLOR_BGRA2BGR);
                    }
                    m_d3dContext->Unmap(m_stagingTexture.get(), 0);
                }
            }
        }
        catch (...) {}
    }

public:
    WGCCapturer() noexcept { try { winrt::init_apartment(winrt::apartment_type::multi_threaded); } catch (...) {} }
    ~WGCCapturer() { StopCapture(); }

    bool StartCapture(HWND hwnd) {
        if (m_isCapturing) return true;
        m_targetHwnd = hwnd;

        RECT er;
        DwmGetWindowAttribute(m_targetHwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &er, sizeof(RECT));
        RECT wr;
        GetWindowRect(m_targetHwnd, &wr);
        POINT pt = { 0, 0 };
        ClientToScreen(m_targetHwnd, &pt);

        m_offsetX = pt.x - er.left;
        m_offsetY = pt.y - wr.top;

        printf("[WGC] 校准数据: 标题栏(Y偏移)=%d, 实体边框(X偏移)=%d\n", m_offsetY, m_offsetX);

        try {
            D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION, m_d3dDevice.put(), nullptr, m_d3dContext.put());
            auto dxgiDevice = m_d3dDevice.as<IDXGIDevice>();
            winrt::com_ptr<::IInspectable> inspectableDevice;
            CreateDirect3D11DeviceFromDXGIDevice(dxgiDevice.get(), inspectableDevice.put());
            m_device = inspectableDevice.as<winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice>();

            auto factory = winrt::get_activation_factory<winrt::Windows::Graphics::Capture::GraphicsCaptureItem, IGraphicsCaptureItemInterop>();
            factory->CreateForWindow(m_targetHwnd, winrt::guid_of<ABI::Windows::Graphics::Capture::IGraphicsCaptureItem>(), winrt::put_abi(m_item));

            m_framePool = winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool::CreateFreeThreaded(
                m_device, winrt::Windows::Graphics::DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized, 2, m_item.Size());

            m_framePool.FrameArrived({ this, &WGCCapturer::OnFrameArrived });
            m_session = m_framePool.CreateCaptureSession(m_item);

            if (winrt::Windows::Foundation::Metadata::ApiInformation::IsPropertyPresent(winrt::name_of<winrt::Windows::Graphics::Capture::GraphicsCaptureSession>(), L"IsBorderRequired"))
                m_session.IsBorderRequired(false);

            m_session.StartCapture();
            m_isCapturing = true;
            return true;
        }
        catch (winrt::hresult_error const& ex) {
            // 捕获具体的 WinRT/COM 异常并打印
            wprintf(L"[WGC 致命错误] WinRT 异常: %s (错误码: 0x%08X)\n", ex.message().c_str(), ex.code().value);
            return false;
        }
        catch (const std::exception& ex) {
            printf("[WGC 致命错误] C++ 标准异常: %s\n", ex.what());
            return false;
        }
        catch (...) {
            printf("[WGC 致命错误] 发生未知异常！\n");
            return false;
        }
    }

    cv::Mat GetArea(RECT r) {
        std::lock_guard<std::mutex> lock(m_frameMutex);
        if (m_latestMat.empty()) return cv::Mat();

        // 将逻辑坐标 r 下移 m_offsetY (跳过标题栏)，右移 m_offsetX (跳过实体边框)
        int realX = (int)r.left + m_offsetX;
        int realY = (int)r.top + m_offsetY;
        int realW = (int)(r.right - r.left);
        int realH = (int)(r.bottom - r.top);

        // 进一步检查是否超出右边界和下边界
        realX = (std::max)(0, (std::min)(realX, m_latestMat.cols - 1));
        realY = (std::max)(0, (std::min)(realY, m_latestMat.rows - 1));
        realW = (std::min)(realW, m_latestMat.cols - realX);
        realH = (std::min)(realH, m_latestMat.rows - realY);

        // 如果计算出的长宽依然无效，则跳过
        if (realW <= 0 || realH <= 0) return cv::Mat();

        return m_latestMat(cv::Rect(realX, realY, realW, realH)).clone();
    }

    void StopCapture() {
        if (m_session) m_session.Close();
        if (m_framePool) m_framePool.Close();
        m_session = nullptr; m_framePool = nullptr; m_isCapturing = false;
        m_stagingTexture = nullptr;
    }

    void SaveDebugImg() {
        std::lock_guard<std::mutex> lock(m_frameMutex);
        if (!m_latestMat.empty()) {
            cv::imwrite("Debug_WGC_Original.bmp", m_latestMat);
        }
    }
};