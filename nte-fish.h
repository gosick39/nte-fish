#pragma once
#include <iostream>
#include <thread>
#include <vector>
#include <windows.h>
#include <opencv2/opencv.hpp>
#include "WGCCapturer.h"
#include "Template.h"
#include "Keyboard.h"

#pragma comment(lib, "opencv_core460.lib")
#pragma comment(lib, "opencv_imgproc460.lib")
#pragma comment(lib, "opencv_imgcodecs460.lib")
#pragma comment(lib, "opencv_highgui460.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

class AutoFishingBot {
public:
    AutoFishingBot();
    ~AutoFishingBot();
    void init();
    void run();
    cv::Mat getScreenshot();
    Keyboard* keyboard = nullptr;

private:
    HWND m_hwnd = nullptr;
    WGCCapturer m_wgcCapturer;
    bool m_useWGC = true;      // 是否使用WGC
    HDC m_hdcDesktop = nullptr; // 用于 GDI 截图

    int m_fishCount = 0;
    std::chrono::steady_clock::time_point m_startTime = std::chrono::steady_clock::now();
    
    void waitUntilWindowFocus();
    void waitUntilAppear(const Template& tpl, double threshold = 0.85);
    void waitUntilAllAppear(const std::vector<Template*>& tpls, double threshold = 0.85);
    std::string waitUntilAnyAppear(const std::vector<Template*>& tpls, double threshold = 0.85);
    bool waitForMatch(const Template& tpl, double timeout_seconds, double threshold = 0.85);
    std::string waitForAnyMatch(const std::vector<Template*>& tpls, double timeout_seconds, double threshold = 0.85);
    bool waitAndClickUntilGone(const Template& tpl, int key, double threshold, double waitTimeout, double clickTimeout);
    void startFishBar(int interval_ms);
    
    // FishBar相关图像分析
    std::pair<int, int> getGreenBar(const cv::Mat& screenshot);
    int getYellowCursor(const cv::Mat& screenshot);
    void waitUntilUiAppear();
};