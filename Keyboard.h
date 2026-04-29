#pragma once
#include <windows.h>
#include <thread>
#include <chrono>

class Keyboard {
public:
    Keyboard(HWND hwnd, bool is_debug) : m_hwnd(hwnd), is_debug(is_debug), current_key(0) {}

    void keepActive() {
        if (GetForegroundWindow() != m_hwnd) {

            // 通知窗口当前是激活状态
            SendMessage(m_hwnd, WM_ACTIVATE, WA_ACTIVE, 0);

            //// 1. 发送激活消息：告诉窗口它现在是活动状态
            //SendMessage(m_hwnd, WM_ACTIVATE, WA_ACTIVE, 0); 
            //// 2. 发送非客户区激活：确保标题栏和边框显示为激活颜色
            //SendMessage(m_hwnd, WM_NCACTIVATE, TRUE, 0);
            //// 3. 模拟获得焦点：让窗口认为键盘输入流已定向到它
            //SendMessage(m_hwnd, WM_SETFOCUS, 0, 0);

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    void press(char key) {
        char k = toupper(key);
        if (current_key == k) return;
        releaseAll();
        doPostMsg(WM_KEYDOWN, k, 0);
        current_key = k;
    }

    void releaseAll() {
        if (current_key) {
            doPostMsg(WM_KEYUP, current_key, 0);
            current_key = 0;
        }
    }

    void click(char key, int duration_ms = 100) {
        int k = (key >= 'a' && key <= 'z') ? toupper(key) : key;
        doPostMsg(WM_KEYDOWN, k, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(duration_ms));
        doPostMsg(WM_KEYUP, k, 0x80000000);
    }

    void mouseClick(int x, int y) {
        LPARAM lParam = MAKELPARAM(x, y);
        doPostMsg(WM_LBUTTONDOWN, MK_LBUTTON, lParam);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        doPostMsg(WM_LBUTTONUP, 0, lParam);
    }

    /**
     * @param x 目标相对坐标 X
     * @param y 目标相对坐标 Y
     * @param move 是否模拟物理光标移动 (对应 move=True)
     * @param duration_ms 按下和抬起之间的延迟
     */
    void mouseClick(int x, int y, bool move = false, int duration_ms = 50) {
        LPARAM lParam = MAKELPARAM(x, y);
        POINT oldPos;

        if (move) {
            // 1. 记录当前真实的物理光标位置
            GetCursorPos(&oldPos);

            // 2. 将相对坐标转换为屏幕绝对坐标
            POINT screenPos = { x, y };
            ClientToScreen(m_hwnd, &screenPos);

            // 3. 物理移动光标并微小延迟，确保游戏逻辑捕获到光标进入
            SetCursorPos(screenPos.x, screenPos.y);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        // 4. 执行后台点击逻辑
        doPostMsg(WM_LBUTTONDOWN, MK_LBUTTON, lParam);

        std::this_thread::sleep_for(std::chrono::milliseconds(duration_ms));

        doPostMsg(WM_LBUTTONUP, 0, lParam);

        if (move) {
            // 5. 操作完成后，将光标移回原来的位置
            std::this_thread::sleep_for(std::chrono::milliseconds(10)); // 给点击留出反应时间
            SetCursorPos(oldPos.x, oldPos.y);
        }
    }

    void doPostMsg(UINT msg, WPARAM wParam, LPARAM lParam) {
        keepActive();
        PostMessage(m_hwnd, msg, wParam, lParam);
        if (is_debug) {
            printf("[DEBUG] 已发送消息: msg=0x%X, wParam=0x%llX, lParam=0x%llX\n",
                (unsigned int)msg,
                (unsigned long long)wParam,
                (unsigned long long)lParam);
        }
	}

    // 按下指定位置，指定延迟
    void mouseClickSendInput(int x, int y, int duration_ms = 50) {
        // 窗口坐标转屏幕坐标
        POINT pos = { x, y };
        POINT res = pos;
        ClientToScreen(m_hwnd, &res);
        x = res.x, y = res.y;

		// 如果窗口非激活，先激活窗口（带到前台），避免点错成其他窗口
        if (GetForegroundWindow() != m_hwnd) {
            SetForegroundWindow(m_hwnd);
        }

        // 执行按键操作
        INPUT Input = { 0 };
        Input.type = INPUT_MOUSE;
        Input.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_LEFTDOWN;
        SetCursorPos(x, y); // 移动鼠标位置
        SendInput(1, &Input, sizeof(INPUT));

        std::this_thread::sleep_for(std::chrono::milliseconds(duration_ms));

        ZeroMemory(&Input, sizeof(INPUT));
        Input.type = INPUT_MOUSE;
        Input.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_LEFTUP;
        SendInput(1, &Input, sizeof(INPUT));
    }

private:
    HWND m_hwnd;
    bool is_debug;
    char current_key;
};