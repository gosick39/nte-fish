#pragma once
#include <windows.h>
#include <thread>
#include <chrono>

class Keyboard {
public:
    Keyboard(HWND hwnd) : m_hwnd(hwnd), current_key(0) {}

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
        }
    }

    void press(char key) {
        char k = toupper(key);
        if (current_key == k) return;
        releaseAll();
        keepActive();
        PostMessage(m_hwnd, WM_KEYDOWN, k, 0);
        current_key = k;
    }

    void releaseAll() {
        if (current_key) {
            keepActive();
            PostMessage(m_hwnd, WM_KEYUP, current_key, 0);
            current_key = 0;
        }
    }

    void click(char key, int duration_ms = 100) {
        int k = (key >= 'a' && key <= 'z') ? toupper(key) : key;
        keepActive();
        PostMessage(m_hwnd, WM_KEYDOWN, k, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(duration_ms));
        keepActive();
        PostMessage(m_hwnd, WM_KEYUP, k, 0x80000000);
    }

    void mouseClick(int x, int y) {
        LPARAM lParam = MAKELPARAM(x, y);
        keepActive();
        PostMessage(m_hwnd, WM_LBUTTONDOWN, MK_LBUTTON, lParam);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        keepActive();
        PostMessage(m_hwnd, WM_LBUTTONUP, 0, lParam);
    }

private:
    HWND m_hwnd;
    char current_key;
};