#pragma once
#include <iostream>
#include <thread>
#include <vector>
#include <windows.h>
#include <opencv2/opencv.hpp>
#include "WGCCapturer.h"
#include "Template.h"
#include "Keyboard.h"
#include "ini.h"

#pragma comment(lib, "opencv_core460.lib")
#pragma comment(lib, "opencv_imgproc460.lib")
#pragma comment(lib, "opencv_imgcodecs460.lib")
#pragma comment(lib, "opencv_highgui460.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

struct TplConfig {
    std::string name;
    int x1, y1, x2, y2;
};

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

    bool is_debug = false; // 是否调试
	bool is_auto_sell = false; // 是否自动卖鱼
	bool is_auto_buy = false; // 是否自动买饵
	bool is_re_link = false; // 是否断线重连
	bool is_cut_tpl = false; // 是否截取模板

    std::map<std::string, Template*> m_templates;   // 动态存储所有模板的指针

    int m_fishCount = 0;
    std::chrono::steady_clock::time_point m_startTime = std::chrono::steady_clock::now();

    void loadTemplatesFromCSV(const std::string& filepath);
    std::vector<Template*> getTemplates();
    std::vector<Template*> getTemplatesWithKeys();
    void startBackgroundMonitor(); // 替代旧的匿名线程，集成退出和中键监控功能
    
    void waitUntilAppear(const Template& tpl, double threshold = 0.85);
    void waitUntilAllAppear(const std::vector<Template*>& tpls, double threshold = 0.85);
    std::string waitUntilAnyAppear(const std::vector<Template*>& tpls, double threshold = 0.85);
    bool waitForMatch(const Template& tpl, double timeout_seconds, double threshold = 0.85);
    bool waitForMatchAndGetCenter(const Template& tpl, cv::Point& outCenter, double timeout_seconds, double threshold);
    bool waitForAllMatch(const std::vector<Template*>& tpls, double timeout_seconds, double threshold = 0.85);
    std::string waitForAnyMatch(const std::vector<Template*>& tpls, double timeout_seconds, double threshold = 0.85);
    void startFishBar(int interval_ms);
    
    // FishBar相关图像分析
    std::pair<int, int> getGreenBar(const cv::Mat& screenshot);
    int getYellowCursor(const cv::Mat& screenshot);
    void waitUntilUiAppear();

    char stringToKeyCode(const std::string& keyStr);
};