#include "nte-fish.h"
#include <fstream>   // 用于文件写入
#include <chrono>    // 用于获取时间
#include <ctime>     // 用于格式化时间

#pragma warning(disable: 4996) // 禁用 ctime 安全警告

AutoFishingBot::AutoFishingBot() {
    m_hdcDesktop = GetDC(GetDesktopWindow());
}

AutoFishingBot::~AutoFishingBot() {
    if (m_hdcDesktop) {
        ReleaseDC(GetDesktopWindow(), m_hdcDesktop);
    }
}
void AutoFishingBot::init() {
    // 脚本信息
    SetConsoleTitle(L"异环-自动钓鱼 v1.2");

    // 提示信息
    std::cout << "    异环-自动钓鱼 v1.2  --by gosick39（幻塔妙妙屋Q群：565943273）\n\n";
    std::cout << "  注意：本程序仅学习使用，禁止商用！\n\n";
    std::cout << "  使用方法：双击打开.exe，进入钓鱼待机页面（不要按F）\n\n";


    //m_hwnd = FindWindowEx(nullptr, nullptr, L"UnrealWindow", nullptr);
    m_hwnd = FindWindowW(L"UnrealWindow", L"异环  ");
    if (!m_hwnd) {
        std::cerr << "请先进入游戏！未找到异环窗口\n";
        system("pause");
        exit(1);
    }

    std::cout << "[WGC] 正在初始化...\n";
    if (!m_wgcCapturer.StartCapture(m_hwnd)) {
        std::cerr << "[WGC] 初始化失败！将尝试使用GDI模式截图\n";
     /*   system("pause");
        exit(1);*/
        m_useWGC = false;
    }
    else {
        m_useWGC = true;
    }

    // 调整窗口大小以确保画面截取比例一致
    RECT rect;
    GetClientRect(m_hwnd, &rect);
    if (rect.right - rect.left != 1280 || rect.bottom - rect.top != 720) {
        RECT windowRect;
        GetWindowRect(m_hwnd, &windowRect);
        int dx = (windowRect.right - windowRect.left) - (rect.right - rect.left);
        int dy = (windowRect.bottom - windowRect.top) - (rect.bottom - rect.top);
        MoveWindow(m_hwnd, windowRect.left, windowRect.top, 1280 + dx, 720 + dy, TRUE);
    }

    keyboard = new Keyboard(m_hwnd);

    // 启动后台监听退出的按键 `
    std::thread([]() {
        while (true) {
            if (GetAsyncKeyState(VK_OEM_3) & 0x8000) {
                std::cout << "\n[退出] 检测到 ` 键，终止程序...\n";
                exit(0);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }).detach();
}

cv::Mat AutoFishingBot::getScreenshot() {
    int width = 1280;
    int height = 720;

    if (m_useWGC) {
        cv::Mat src = m_wgcCapturer.GetArea({ 0, 0, width, height });
        if (src.empty()) {
            src.create(height, width, CV_8UC4);
            src = cv::Scalar(0, 0, 0, 255);
        }
        return src;
    }

    cv::Mat src(height, width, CV_8UC4);

    // 获取游戏窗口在屏幕上的绝对坐标
    RECT screenRect;
    GetClientRect(m_hwnd, &screenRect);
    POINT p1 = { screenRect.left, screenRect.top };
    ClientToScreen(m_hwnd, &p1);

    HDC hwindowCompatibleDC = CreateCompatibleDC(m_hdcDesktop);
    HBITMAP hbwindow = CreateCompatibleBitmap(m_hdcDesktop, width, height);
    SelectObject(hwindowCompatibleDC, hbwindow);

    // 从屏幕拷贝像素
    BitBlt(hwindowCompatibleDC, 0, 0, width, height, m_hdcDesktop, p1.x, p1.y, SRCCOPY);

    // 拷贝到 OpenCV Mat
    BITMAPINFOHEADER bi = { sizeof(BITMAPINFOHEADER), width, -height, 1, 32, BI_RGB, 0, 0, 0, 0, 0 };
    GetDIBits(hwindowCompatibleDC, hbwindow, 0, height, src.data, (BITMAPINFO*)&bi, DIB_RGB_COLORS);

    // 清理句柄防止内存泄漏
    DeleteObject(hbwindow);
    DeleteDC(hwindowCompatibleDC);

    // 转换为 BGR (OpenCV 标准)
    cv::Mat bgr;
    cv::cvtColor(src, bgr, cv::COLOR_BGRA2BGR);
    //cv::imwrite("src_rgb.png", bgr);
    return bgr;
}

void AutoFishingBot::waitUntilWindowFocus() {
    int loopCount = 0;
    auto startTime = std::chrono::steady_clock::now();
    if (GetForegroundWindow() != m_hwnd) {
        std::cout << "[状态] 正在等待窗口聚焦...\n";
    }
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (GetForegroundWindow() == m_hwnd) {
            break;
        }
    }
}

void AutoFishingBot::waitUntilAppear(const Template& tpl, double threshold) {
    waitUntilWindowFocus();
    int loopCount = 0;
    while (true) {
        cv::Mat frame = getScreenshot();

        if (tpl.match(frame)) {
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        loopCount++;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

std::string AutoFishingBot::waitUntilAnyAppear(const std::vector<Template*>& tpls, double threshold) {
    waitUntilWindowFocus();
    int loopCount = 0;

    while (true) {
        cv::Mat frame = getScreenshot();

        // 核心逻辑：遍历所有模板，共用同一张截图 frame
        for (int i = 0; i < tpls.size(); ++i) {
            if (tpls[i]->match(frame, threshold)) {
                return tpls[i]->getTplName();
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        loopCount++;
    }
}

bool AutoFishingBot::waitForMatch(const Template& tpl, double timeout_seconds, double threshold) {
    waitUntilWindowFocus();

    // 记录开始时间
    auto startTime = std::chrono::steady_clock::now();
    int loopCount = 0;

    while (true) {
        // 1. 获取当前截图并进行匹配
        cv::Mat frame = getScreenshot();
        if (tpl.match(frame, threshold)) {
            return true;
        }

        // 2. 计算已耗时
        auto currentTime = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(currentTime - startTime).count();

        // 3. 检查是否超时
        if (elapsed >= timeout_seconds) {
            return false;
        }

        // 适当休眠减少 CPU 占用
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        loopCount++;
    }
}

std::string AutoFishingBot::waitForAnyMatch(const std::vector<Template*>& tpls, double timeout_seconds, double threshold) {
    waitUntilWindowFocus();

    auto startTime = std::chrono::steady_clock::now();
    int loopCount = 0;

    while (true) {
        cv::Mat frame = getScreenshot();

        // 1. 遍历匹配所有模板
        for (int i = 0; i < tpls.size(); ++i) {
            if (tpls[i]->match(frame, threshold)) {
                return tpls[i]->getTplName(); // 返回第一个匹配成功的索引
            }
        }

        // 2. 检查超时
        auto currentTime = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(currentTime - startTime).count();

        if (elapsed >= timeout_seconds) {
            return "";
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        loopCount++;
    }
}

// ---------------------- Fish Bar 逻辑 ---------------------- //

std::pair<int, int> AutoFishingBot::getGreenBar(const cv::Mat& screenshot) {
    waitUntilWindowFocus();
    cv::Rect roiRect(412, 46, 475, 7);
    cv::Mat roi = screenshot(roiRect).clone();
    // 确保是 BGR 格式以便画彩色标记
    if (roi.channels() == 4) cv::cvtColor(roi, roi, cv::COLOR_BGRA2BGR);
    cv::Vec3b target(173, 202, 42); // OpenCV 中 BGR 的匹配

    std::vector<int> cols;
    for (int x = 0; x < roi.cols; ++x) {
        bool col_match = false;
        for (int y = 0; y < roi.rows; ++y) {
            cv::Vec3b p = roi.at<cv::Vec3b>(y, x);
            // 距离计算公式等同于 numpy 的 sum(abs(roi - target), axis=2)
            int dist = std::abs(p[0] - target[0]) + std::abs(p[1] - target[1]) + std::abs(p[2] - target[2]);
            if (dist < 80) {
                col_match = true;
                break;
            }
        }
        if (col_match) {
            cols.push_back(x);
        }
    }

    if (!cols.empty()) {
        int left = cols.front();
        int right = cols.back();
        int width = right - left;
        return { left + 412 + width / 3, left + 412 + static_cast<int>(width / 1.5) }; // 返回全屏坐标
    }
    return {-1, -1};
}

int AutoFishingBot::getYellowCursor(const cv::Mat& screenshot) {
    cv::Rect roiRect(412, 46, 475, 7);
    cv::Mat roi = screenshot(roiRect).clone();
    cv::Vec3b target(157, 246, 254); // BGR

    std::vector<int> cols;
    for (int x = 0; x < roi.cols; ++x) {
        bool col_match = false;
        for (int y = 0; y < roi.rows; ++y) {
            cv::Vec3b p = roi.at<cv::Vec3b>(y, x);
            int dist = std::abs(p[0] - target[0]) + std::abs(p[1] - target[1]) + std::abs(p[2] - target[2]);
            if (dist < 80) {
                col_match = true;
                break;
            }
        }
        if (col_match) {
            cols.push_back(x);
        }
    }

    if (!cols.empty()) {
        int center = (cols.front() + cols.back()) / 2;
        return center + 412; // 返回全屏坐标
    }
    return -1;
}

void AutoFishingBot::waitUntilUiAppear() {
    while (true) {
        cv::Mat frame = getScreenshot();
        if (getGreenBar(frame).first != -1 && getYellowCursor(frame) != -1) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void AutoFishingBot::startFishBar() {
    waitUntilUiAppear();

    int missing_green_bar_count = 0;
    while (true) {
        cv::Mat frame = getScreenshot();
        auto green_bar = getGreenBar(frame);

        if (green_bar.first == -1) {
            missing_green_bar_count++;
            if (missing_green_bar_count > 10) break; // 连续 10 帧检测不到绿条才结束
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        missing_green_bar_count = 0;
        int left = green_bar.first;
        int right = green_bar.second;
        int cursor = getYellowCursor(frame);

        if (cursor == -1) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        if (cursor < left) {
            keyboard->press('D');
        } else if (cursor > right) {
            keyboard->press('A');
        } else {
            keyboard->releaseAll();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    keyboard->releaseAll();
}

// ---------------------- 主运行循环 ---------------------- //

void AutoFishingBot::run() {
    Template t_start("./templates/START.png", 1018, 616, 1127, 638);
    Template t_ready("./templates/READY.png", 1163, 628, 1209, 671);
    Template t_catch("./templates/CATCH.png", 515, 166, 785, 186);
    Template t_close("./templates/CLOSE.png", 573, 646, 711, 661);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    //cv::Mat frame = getScreenshot();
    //t_start.saveDebugImg(frame);

    std::cout << "[主循环] 等待抛竿状态...\n";

  //  if (waitForMatch(t_start, 3)) {
  //      std::cout << "[系统] 识别到 开始(START)，进入待机\n";
  //      keyboard->mouseClick(t_start.getCenterX(), t_start.getCenterY());
		//Sleep(200); 
  //      keyboard->mouseClick(t_start.getCenterX(), t_start.getCenterY());
  //  }

    while (true) {
        waitUntilAppear(t_ready);
        std::cout << "[系统] 识别到 就绪(READY)，开始抛竿\n";
        keyboard->click('F');

        waitUntilAppear(t_catch);
        std::cout << "[系统] 识别到 咬钩(CATCH)，开始拉鱼\n";
        keyboard->click('F');
        startFishBar();

        if (waitForMatch(t_close, 3.5, 0.85)) {
            // 增加计数
            m_fishCount++;

            // 计算时长
            auto now = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - m_startTime).count();
            int minutes = static_cast<int>(duration / 60);
            int seconds = static_cast<int>(duration % 60);

            std::cout << "[系统] 识别到 结束(CLOSE)，关闭页面\n";
            keyboard->click(VK_ESCAPE);

            std::cout << "[系统] 本轮结束 | 已运行: " << minutes << "分" << seconds << "秒 | 总收获: " << m_fishCount << " 条" << std::endl;
        }
        else {
            std::cout << "[系统] 很遗憾，本轮未钓到鱼" << std::endl;

        }
    }
}

//int main() {
//    SetProcessDPIAware();
//    AutoFishingBot bot;
//    bot.init();
//    bot.run();
//    return 0;
//}

// ---------------------- 全局崩溃日志记录功能 ---------------------- //

// 写入日志的辅助函数
void WriteCrashLog(const std::string& message) {
    std::ofstream logFile("crash_log.txt", std::ios::app); // 追加模式
    if (logFile.is_open()) {
        // 获取当前时间
        auto now = std::chrono::system_clock::now();
        std::time_t now_c = std::chrono::system_clock::to_time_t(now);

        logFile << "========================================\n";
        logFile << "崩溃时间: " << std::ctime(&now_c); // ctime自带换行符
        logFile << message << "\n";
        logFile << "========================================\n\n";
        logFile.flush();
        logFile.close();
    }
}

// Windows 系统级崩溃捕获 (SEH 异常, 如空指针、内存越界闪退)
LONG WINAPI UnhandledExceptionHandler(EXCEPTION_POINTERS* ExceptionInfo) {
    std::string crashMsg = "发生系统级异常 (SEH)!\n";

    // 获取异常代码并转为十六进制字符串
    char buffer[128];
    sprintf(buffer, "异常代码: 0x%08X\n异常地址: 0x%p",
        ExceptionInfo->ExceptionRecord->ExceptionCode,
        ExceptionInfo->ExceptionRecord->ExceptionAddress);

    crashMsg += buffer;
    WriteCrashLog(crashMsg);

    // 弹出提示框告知用户（可选）
    MessageBoxW(NULL, L"程序遇到严重错误已崩溃！详细信息已保存至 crash_log.txt", L"致命错误", MB_ICONERROR | MB_OK);

    return EXCEPTION_EXECUTE_HANDLER; // 允许程序关闭
}

int main() {
    // 1. 注册 Windows 系统级未处理异常钩子
    SetUnhandledExceptionFilter(UnhandledExceptionHandler);

    // 2. 使用 try-catch 包裹主逻辑，捕获 C++ 层面抛出的异常
    try {
        SetProcessDPIAware();
        AutoFishingBot bot;
        bot.init();
        bot.run();
    }
    catch (const cv::Exception& e) {
        // 捕获 OpenCV 专属异常
        std::string msg = "发生 OpenCV 异常:\n" + std::string(e.what());
        WriteCrashLog(msg);
        MessageBoxW(NULL, L"发生 OpenCV 图像处理异常，日志已保存。", L"错误", MB_ICONERROR);
    }
    catch (const std::exception& e) {
        // 捕获 C++ 标准异常
        std::string msg = "发生 C++ 标准异常:\n" + std::string(e.what());
        WriteCrashLog(msg);
        MessageBoxW(NULL, L"发生标准异常，日志已保存。", L"错误", MB_ICONERROR);
    }
    catch (...) {
        // 捕获所有其他未知的 C++ 异常
        WriteCrashLog("发生未知的 C++ 异常!");
        MessageBoxW(NULL, L"发生未知异常，日志已保存。", L"错误", MB_ICONERROR);
    }

    return 0;
}