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
    SetConsoleTitle(L"异环-自动钓鱼 v1.6");

    // 提示信息
    std::cout << "    异环-自动钓鱼 v1.6  --by gosick39（Q群：565943273）\n\n";
    std::cout << "  注意：本程序仅学习使用，禁止商用！\n\n";
    std::cout << "  使用方法：进入钓鱼待机状态，双击打开.exe，退出键`\n\n";

    // 加载config.ini
    ZIni ini("config.ini");
    is_debug = ini.getInt("common", "is_debug", 0);
    is_auto_sell = ini.getInt("common", "is_auto_sell", 0);
    is_auto_buy = ini.getInt("common", "is_auto_buy", 0);
    is_re_link = ini.getInt("common", "is_re_link", 0);
    is_cut_tpl = ini.getInt("common", "is_cut_tpl", 0);
    std::cout << "[config] is_debug = " << is_debug << std::endl;
    std::cout << "[config] is_auto_sell = " << is_auto_sell << std::endl;
    std::cout << "[config] is_auto_buy = " << is_auto_buy << std::endl;
    std::cout << "[config] is_re_link = " << is_re_link << std::endl;
    std::cout << "[config] is_cut_tpl = " << is_cut_tpl << std::endl;
    std::cout << std::endl;

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
    std::cout << std::endl;

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

	// 调整控制台窗口位置
    HWND hConsole = GetConsoleWindow();
    if (hConsole) {
        RECT gameRect;
        GetWindowRect(m_hwnd, &gameRect);
        // 控制台尺寸
        const int consoleWidth = 500;
        const int consoleHeight = gameRect.bottom - gameRect.top;
        // 放在游戏窗口，间距 1px
        int x = gameRect.left + 1;
        int y = gameRect.top;
        SetWindowPos(
            hConsole,
            NULL,
            x,
            y,
            consoleWidth,
            consoleHeight,
            SWP_NOZORDER | SWP_NOACTIVATE
        );
    }

    keyboard = new Keyboard(m_hwnd, is_debug);

    // 启动后台监听线程
    startBackgroundMonitor();

    if (is_cut_tpl) {
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(3));
        }
    }

    // 加载外部模板 CSV (假设表头为: 模板名称,x1,y1,x2,y2)
    loadTemplatesFromCSV("templates.csv");
}

void AutoFishingBot::loadTemplatesFromCSV(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "[错误] 未找到配置文件: " << filepath << std::endl;
        return;
    }

    std::string line;
    std::getline(file, line); // 跳过表头

    while (std::getline(file, line)) {
        if (line.empty()) continue;
        std::stringstream ss(line);
        std::string name, x1s, y1s, x2s, y2s, keyStr;

        std::getline(ss, name, ',');
        std::getline(ss, x1s, ',');
        std::getline(ss, y1s, ',');
        std::getline(ss, x2s, ',');
        std::getline(ss, y2s, ',');
        std::getline(ss, keyStr, ','); // 读取新增的按键列

        int x1 = std::stoi(x1s);
        int y1 = std::stoi(y1s);
        int x2 = std::stoi(x2s);
        int y2 = std::stoi(y2s);

        // 处理按键：如果为空则为0，否则取第一个字符（如 'F', 'Q'）
        char defKey = stringToKeyCode(keyStr);

        std::string fullPath = "./templates/" + name;
        if (!std::filesystem::exists(fullPath)) {
            std::cerr << "[警告] 模板文件不存在: " << fullPath << std::endl;
            continue;
        }

        // 存入 map
        m_templates[name] = new Template(fullPath, x1, y1, x2, y2);
        m_templates[name]->setIsDebug(is_debug);
        m_templates[name]->setDefaultKey(defKey);
    }
}

// 获取模板集合
std::vector<Template*> AutoFishingBot::getTemplates() {
    std::vector<Template*> result;
    for (auto const& [name, tpl] : m_templates) {
        result.push_back(tpl);
    }
    return result;
}

// 获取带默认按键的模板集合
std::vector<Template*> AutoFishingBot::getTemplatesWithKeys() {
    std::vector<Template*> result;
    for (auto const& [name, tpl] : m_templates) {
        if (tpl->getDefaultKey() != 0) {
            result.push_back(tpl);
        }
    }
    return result;
}

void AutoFishingBot::startBackgroundMonitor() {
    std::thread([this]() {
        bool isFirstPointSet = false;
        POINT p1 = { 0, 0 };

        while (true) {
            // 1. 监听退出键 `
            if (GetAsyncKeyState(VK_OEM_3) & 0x8000) {
                std::cout << "\n[退出] 程序终止..." << std::endl;
                exit(0);
            }

            // 2. 监听鼠标中键按下 (使用 GetAsyncKeyState 的单次点击检测逻辑)
            static bool lastState = false;
            bool currentState = (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0;

            // 检测按下瞬间（从未按下变为按下）
            if (currentState && !lastState) {
                POINT currentPos;
                GetCursorPos(&currentPos);
                ScreenToClient(m_hwnd, &currentPos); // 转换为窗口相对坐标

                if (!isFirstPointSet) {
                    // 第一次按下：记录起点
                    p1 = currentPos;
                    isFirstPointSet = true;
                    std::cout << "[坐标采集] 已设置起点: (" << p1.x << ", " << p1.y << ")，请移动鼠标到终点再次点击中键..." << std::endl;
                }
                else {
                    // 第二次按下：计算矩形并截图
                    POINT p2 = currentPos;
                    isFirstPointSet = false; // 重置状态

                    if (p1.x == p2.x && p1.y == p2.y) {
                        std::cout << "[坐标采集] 两次点击位置相同，当前坐标: (" << p2.x << ", " << p2.y << ")" << std::endl;
                    }
                    else {
                        // 确定矩形坐标 (处理反向点击的情况)
                        int x1 = std::min(p1.x, p2.x);
                        int y1 = std::min(p1.y, p2.y);
                        int x2 = std::max(p1.x, p2.x);
                        int y2 = std::max(p1.y, p2.y);

                        std::cout << "[坐标采集] 矩形完成! 坐标: " << x1 << ", " << y1 << ", " << x2 << ", " << y2 << std::endl;

                        // 获取截图并裁剪保存[cite: 1, 2]
                        cv::Mat frame = getScreenshot();
                        cv::Rect roi(x1, y1, x2 - x1, y2 - y1);

                        // 防止越界检测
                        roi = roi & cv::Rect(0, 0, frame.cols, frame.rows);

                        if (roi.width > 0 && roi.height > 0) {
                            cv::Mat crop = frame(roi).clone();
                            std::string filename = "tpl_capture_" + std::to_string(time(nullptr)) + ".png";
                            cv::imwrite(filename, crop);
                            std::cout << "[截图保存] 区域已保存至: " << filename << std::endl;
                        }
                    }
                }
            }
            lastState = currentState;
            std::this_thread::sleep_for(std::chrono::milliseconds(50)); // 降低 CPU 占用并防止连击抖动
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

void AutoFishingBot::waitUntilAppear(const Template& tpl, double threshold) {
    while (true) {
        cv::Mat frame = getScreenshot();
        if (tpl.match(frame)) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

void AutoFishingBot::waitUntilAllAppear(const std::vector<Template*>& tpls, double threshold) {
    while (true) {
        cv::Mat frame = getScreenshot();
		bool allMatch = false;
        for (int i = 0; i < tpls.size(); ++i) {
            if (tpls[i]->match(frame, threshold)) {
                allMatch = true;
            }
        }
        if (allMatch) {
            break;
		}
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

std::string AutoFishingBot::waitUntilAnyAppear(const std::vector<Template*>& tpls, double threshold) {
    while (true) {
        cv::Mat frame = getScreenshot();

        // 核心逻辑：遍历所有模板，共用同一张截图 frame
        for (int i = 0; i < tpls.size(); ++i) {
            if (tpls[i]->match(frame, threshold)) {
                return tpls[i]->getTplName();
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

bool AutoFishingBot::waitForMatch(const Template& tpl, double timeout_seconds, double threshold) {
    // 记录开始时间
    auto startTime = std::chrono::steady_clock::now();

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
    }
}

/**
 * 等待模板匹配成功，并返回其在全屏截图中的中心点坐标
 * @param tpl 模板对象
 * @param outCenter [输出] 匹配成功时的中心点坐标
 * @param timeout_seconds 超时时间（秒）
 * @param threshold 相似度阈值
 * @return 是否在超时前匹配成功
 */
bool AutoFishingBot::waitForMatchAndGetCenter(const Template& tpl, cv::Point& outCenter, double timeout_seconds, double threshold) {
    auto startTime = std::chrono::steady_clock::now();

    while (true) {
        // 1. 获取当前截图
        cv::Mat frame = getScreenshot();

        // 2. 调用带有位置返回功能的匹配方法
        cv::Rect matchedRect;
        // 假设你在 Template 类中实现了之前建议的 matchAndGetRect 方法
        if (tpl.matchAndGetRect(frame, matchedRect, threshold)) {
            // 计算中心点：左上角坐标 + 尺寸的一半
            outCenter.x = matchedRect.x + matchedRect.width / 2;
            outCenter.y = matchedRect.y + matchedRect.height / 2;
            return true;
        }

        // 3. 超时检查
        auto currentTime = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(currentTime - startTime).count();
        if (elapsed >= timeout_seconds) {
            return false;
        }

        // 4. 休眠以降低 CPU 负载
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

bool AutoFishingBot::waitForAllMatch(const std::vector<Template*>& tpls, double timeout_seconds, double threshold) {
    auto startTime = std::chrono::steady_clock::now();

    while (true) {
        cv::Mat frame = getScreenshot();

        bool allMatch = false;
        for (int i = 0; i < tpls.size(); ++i) {
            if (tpls[i]->match(frame, threshold)) {
                allMatch = true;
            }
        }
        if (allMatch) {
            break;
        }

        auto currentTime = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(currentTime - startTime).count();

        if (elapsed >= timeout_seconds) {
            return false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
	return true;    
}

std::string AutoFishingBot::waitForAnyMatch(const std::vector<Template*>& tpls, double timeout_seconds, double threshold) {
    auto startTime = std::chrono::steady_clock::now();

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
    }
}

// ---------------------- Fish Bar 逻辑 ---------------------- //

std::pair<int, int> AutoFishingBot::getGreenBar(const cv::Mat& screenshot) {
    if (screenshot.empty()) return { -1, -1 };
    cv::Rect roiRect(412, 46, 475, 7);
    cv::Mat roi = screenshot(roiRect);

    cv::Vec3b target(173, 202, 42); // BGR

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
    if (screenshot.empty()) return -1;
    cv::Rect roiRect(412, 46, 475, 7);
    cv::Mat roi = screenshot(roiRect);

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

void AutoFishingBot::startFishBar(int interval_ms) {

    int missing_green_bar_count = 0;
    while (true) {
        //auto t1 = std::chrono::high_resolution_clock::now();

        cv::Mat frame = getScreenshot();

        //auto t2 = std::chrono::high_resolution_clock::now();

        auto green_bar = getGreenBar(frame);
        int cursor = getYellowCursor(frame);

        //// 计算耗时
        //auto t3 = std::chrono::high_resolution_clock::now();
        //auto cap_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
        //auto det_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t3 - t2).count();
        //std::cout << "[Timer] 截图: " << cap_ms << "ms | 识别: " << det_ms << "ms" << std::endl;

        // --- 逻辑判断 ---
        if (green_bar.first == -1 || cursor == -1) {
            missing_green_bar_count++;
            if (missing_green_bar_count > 10) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
            continue;
        }
        missing_green_bar_count = 0;

        if (cursor < green_bar.first + 2) {
            keyboard->press('D');
        }
        else if (cursor > green_bar.second - 2) {
            keyboard->press('A');
        }
        else {
            keyboard->releaseAll();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
    }
    keyboard->releaseAll();
}

char AutoFishingBot::stringToKeyCode(const std::string& keyStr) {
    if (keyStr.empty()) return 0;

    // 转为大写以便匹配
    std::string s = keyStr;
    std::transform(s.begin(), s.end(), s.begin(), ::toupper);

    // 1. 处理特殊功能键
    if (s == "ESC") return VK_ESCAPE;      // 0x1B
    if (s == "SPACE") return VK_SPACE;    // 0x20
    if (s == "ENTER") return VK_RETURN;   // 0x0D
    if (s == "TAB") return VK_TAB;        // 0x09
    if (s == "SHIFT") return VK_SHIFT;
    if (s == "CTRL") return VK_CONTROL;
    if (s == "ALT") return VK_MENU;

    // 2. 处理如果是数字字符串（如 "27" 直接代表 ESC）
    if (isdigit(s[0])) {
        return (char)std::stoi(s);
    }

    // 3. 默认返回第一个字符（如 'F', 'Q', 'R'）
    return s[0];
}

// ---------------------- 主运行循环 ---------------------- //

void AutoFishingBot::run() {
    /*
    Template t_start1("./templates/START1.png", 1034, 616, 1120, 638, is_debug);
    Template t_start2("./templates/START2.png", 1034, 616, 1120, 638, is_debug);
    //Template t_sure("./templates/SURE.png", 753, 464, 793, 485, is_debug);
    Template t_login("./templates/LOGIN.png", 40, 50, 90, 95, is_debug);
    Template t_enter("./templates/ENTER.png", 775, 383, 832, 402, is_debug);
    Template t_ready1("./templates/READY1.png", 923, 642, 951, 672, is_debug);
    Template t_ready2("./templates/READY2.png", 996, 642, 1025, 672, is_debug);
    Template t_catch("./templates/CATCH.png", 515, 166, 785, 186, is_debug);
    Template t_close("./templates/CLOSE.png", 573, 646, 711, 661, is_debug);
    Template t_full("./templates/FULL.png", 462, 351, 600, 368, is_debug);
    Template t_empty("./templates/EMPTY.png", 531, 350, 751, 368, is_debug);
    // 万能鱼饵图标位置（可能每个人的不一样）： 340, 182, 406, 234
    //Template t_wnye("./templates/WNYE.bmp", 35, 89, 439, 563, is_debug);
    Template t_wnye("./templates/WNYE.png", 340, 182, 406, 234, is_debug);
    std::cout << std::endl;
    */

    Template* t_start1 = m_templates["START1.png"];
    Template* t_start2 = m_templates["START2.png"];
    Template* t_login = m_templates["LOGIN.png"];
    Template* t_enter = m_templates["ENTER.png"];
    Template* t_ready1 = m_templates["READY1.png"];
    Template* t_ready2 = m_templates["READY2.png"];
    Template* t_catch = m_templates["CATCH.png"];
    Template* t_close = m_templates["CLOSE.png"];
    Template* t_full = m_templates["FULL.png"];
    Template* t_gjcs = m_templates["GJCS.png"];
    Template* t_empty = m_templates["EMPTY.png"];
    Template* t_wnye = m_templates["WNYE.png"];

    //t_close->saveDebugImg(getScreenshot());

    // 校验核心模板，防止未配置造成的 nullptr 闪退
    if (!t_start1 || !t_ready1 || !t_catch || !t_close) {
        std::cerr << "[错误] CSV配置中缺失关键钓鱼模板，无法运行主逻辑！\n";
        return;
    }
    std::cout << std::endl;

    std::cout << "[系统] 初始化成功，将自动最小化...\n\n";

    std::this_thread::sleep_for(std::chrono::milliseconds(2500));

    // 最小化控制台窗口
    ShowWindow(GetConsoleWindow(), SW_MINIMIZE);

    // 激活游戏窗口到前台
    SetForegroundWindow(m_hwnd);
    BringWindowToTop(m_hwnd);
    //keyboard->keepActive();

    std::cout << "[系统] 等待抛竿状态...\n";

    while (true) {
		std::vector<Template*> firstTargets = { t_ready1, t_ready2, t_close };
        if (is_re_link) {
            firstTargets.push_back(t_login);
            firstTargets.push_back(t_enter);
            firstTargets.push_back(t_start1);
            firstTargets.push_back(t_start2);
        }
        // 其他额外添加的模板
        for (auto const& [name, tpl] : m_templates) {
            if (tpl->getDefaultKey() != 0) {
                firstTargets.push_back(tpl);
            }
        }

        std::string curr = waitUntilAnyAppear(firstTargets, 0.85);
        if (curr == "READY1.png" || curr == "READY2.png") {
            std::cout << "[就绪] 开始抛竿\n";
            //std::this_thread::sleep_for(std::chrono::milliseconds(500));
            keyboard->click('F');
        }
        else if (curr == "CLOSE.png") {
            // 增加计数
            m_fishCount++;

            // 计算时长
            auto now = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - m_startTime).count();
            int minutes = static_cast<int>(duration / 60);
            int seconds = static_cast<int>(duration % 60);

            std::cout << "[结束] 关闭页面 | 已运行: " << minutes << "分" << seconds << "秒 | 总收获: " << m_fishCount << " 条" << std::endl;

            keyboard->click(VK_ESCAPE);
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }
        else if (curr == "LOGIN.png") {
            std::cout << "[系统] 识别到 登录(LOGIN)，可能是断线了，正在尝试点击登录...\n";
            keyboard->mouseClickSendInput(640, 621, 100);
            std::this_thread::sleep_for(std::chrono::seconds(10));
            continue;
        }
        else if (curr == "ENTER.png") {
            std::cout << "[系统] 识别到 F钓鱼(ENTER)，按F进入钓鱼\n";
            keyboard->click('F');
            std::this_thread::sleep_for(std::chrono::seconds(2));
            continue;
		}
        else if (curr == "START1.png" || curr == "START2.png") {
            std::cout << "[系统] 识别到 开始(START)，进入待机\n";
            keyboard->mouseClickSendInput(t_start1->getCenterX(), t_start1->getCenterY(), 100);
            std::this_thread::sleep_for(std::chrono::seconds(2));
            continue;
		}
        // 判断 curr 是否在有默认 key 的模板集合中
        else if (m_templates.count(curr) && m_templates[curr]->getDefaultKey() != 0) {
			char key = m_templates[curr]->getDefaultKey();
            std::cout << "[匹配成功] " << curr << "  按键码：" << static_cast<int>(key) << std::endl;
            keyboard->click(key);
            std::this_thread::sleep_for(std::chrono::seconds(2));
            continue;
        }
        else {
            std::cout << "[系统] 等待抛竿状态时识别到未知页面: " << curr << "，请检查游戏界面是否异常\n";
            std::this_thread::sleep_for(std::chrono::seconds(10));
            continue;
		}

		// 正常抛竿后8秒内要识别到咬钩，否则可能是抛竿失败或已无鱼饵或已满仓
        std::vector<Template*> secondTargets = { t_catch };
        if (is_auto_sell) {
            secondTargets.push_back(t_full);
        }
        if (is_auto_buy) {
            secondTargets.push_back(t_empty);
        }
        curr = waitForAnyMatch(secondTargets, 15, 0.85);
        if (curr == "CATCH.png") {
            std::cout << "[咬钩] 开始拉鱼\n";
            keyboard->click('F');
            startFishBar(50);
            // 拉鱼结束后等待2.5秒，确保界面稳定再进行下一步识别，防止界面切换过快导致的误识别
            std::this_thread::sleep_for(std::chrono::milliseconds(2500));
        } 
        else if (curr == "FULL.png") {
            std::cout << "[满仓] 仓库已满，自动卖鱼中，请勿操作鼠标...\n";
            // TODO 卖鱼
            keyboard->click('Q');
            std::this_thread::sleep_for(std::chrono::seconds(3));
            keyboard->mouseClickSendInput(108, 270, 100);
            std::this_thread::sleep_for(std::chrono::seconds(3));
            keyboard->mouseClickSendInput(710, 641, 100);
            std::this_thread::sleep_for(std::chrono::seconds(3));
            keyboard->mouseClickSendInput(779, 471, 100);
            std::this_thread::sleep_for(std::chrono::seconds(3));
            // 前往高价出售
            if (waitForMatch(*t_gjcs, 6, 0.85)) {
                std::cout << "[卖鱼] 识别到可前往高价出售...\n";
                keyboard->mouseClickSendInput(t_gjcs->getCenterX(), t_gjcs->getCenterY(), 100);
                std::this_thread::sleep_for(std::chrono::seconds(2));
                std::cout << "[卖鱼] 高价出售快捷提交...\n";
                keyboard->mouseClickSendInput(t_gjcs->getCenterX(), t_gjcs->getCenterY(), 100);
                std::this_thread::sleep_for(std::chrono::seconds(2));
            }
            std::cout << "[卖鱼] 自动卖鱼完成，返回待机页面...\n";
            // 返回待机页面两次
            for (int i = 0; i < 2; i++) {
                // 如果2秒内没识别到待机页就按ESC
                if (waitForAllMatch({ &t_ready1, &t_ready2 }, 2, 0.8)) {
                    break;
                }
                keyboard->click(VK_ESCAPE);
            }       
            continue;
        }
        else if (curr == "EMPTY.png") {
            std::cout << "[无饵] 已无鱼饵，自动买饵中，请勿操作鼠标...\n";
            // TODO 买饵
            keyboard->click('R');
            std::this_thread::sleep_for(std::chrono::seconds(2));
            // 识别万能鱼饵
            cv::Point clickPos;
			bool isMatch = waitForMatchAndGetCenter(*t_wnye, clickPos, 2, 0.9);
            if (isMatch) {
                keyboard->mouseClickSendInput(clickPos.x, clickPos.y, 100);
                std::this_thread::sleep_for(std::chrono::seconds(2));
                keyboard->mouseClickSendInput(1218, 635, 100);
                std::this_thread::sleep_for(std::chrono::seconds(2));
                keyboard->mouseClickSendInput(1077, 684, 100);
                std::this_thread::sleep_for(std::chrono::seconds(2));
                keyboard->mouseClickSendInput(770, 474, 100);
                std::this_thread::sleep_for(std::chrono::seconds(3));
                std::cout << "[买饵] 自动买饵完成，返回待机页面...\n";
                // 返回待机页面两次
                for (int i = 0; i < 2; i++) {
                    // 如果2秒内没识别到待机页就按ESC
                    if (waitForAllMatch({ &t_ready1, &t_ready2 }, 2, 0.8)) {
                        break;
                    }
                    keyboard->click(VK_ESCAPE);
                }
                std::cout << "[买饵] 切换鱼饵到万能鱼饵...\n";
                std::this_thread::sleep_for(std::chrono::seconds(2));
                keyboard->click('E');
                std::this_thread::sleep_for(std::chrono::seconds(2));
                keyboard->mouseClickSendInput(780, 468, 100);
                std::this_thread::sleep_for(std::chrono::seconds(2));
                std::cout << "[买饵] 切换鱼饵完成，返回待机页面...\n";
                continue;
            }
            else {
                std::cout << "[买饵] 未识别到万能鱼饵（预设位置2，可手动截取自己的万能鱼饵模板并更新csv），返回待机页面...\n";
                // 返回待机页面两次
                for (int i = 0; i < 2; i++) {
                    // 如果2秒内没识别到待机页就按ESC
                    if (waitForAllMatch({ &t_ready1, &t_ready2 }, 2, 0.8)) {
                        break;
                    }
                    keyboard->click(VK_ESCAPE);
                }
                continue;
            }
        }
        else {
            std::cout << "[系统] 15秒内未识别到 咬钩(CATCH)，可能是抛竿失败或已无鱼饵，将重新抛竿...\n";
            continue;
		}
    }
}

//int main() {
//    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
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
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
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