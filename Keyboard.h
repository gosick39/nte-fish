#pragma once
#include <windows.h>
#include <thread>
#include <chrono>

class Keyboard {
public:
    Keyboard(HWND hwnd) : m_hwnd(hwnd), current_key(0) {}

    void press(char key) {
        char k = toupper(key);
        if (current_key == k) return;
        releaseAll();
        PostMessage(m_hwnd, WM_KEYDOWN, k, 0);
        current_key = k;
    }

    void releaseAll() {
        if (current_key) {
            PostMessage(m_hwnd, WM_KEYUP, current_key, 0);
            current_key = 0;
        }
    }

    void click(char key, int duration_ms = 100) {
        int k = (key >= 'a' && key <= 'z') ? toupper(key) : key;
        PostMessage(m_hwnd, WM_KEYDOWN, k, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(duration_ms));
        PostMessage(m_hwnd, WM_KEYUP, k, 0x80000000);
    }

    void mouseClick(int x, int y) {
        LPARAM lParam = MAKELPARAM(x, y);
        PostMessage(m_hwnd, WM_LBUTTONDOWN, MK_LBUTTON, lParam);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        PostMessage(m_hwnd, WM_LBUTTONUP, 0, lParam);
    }

private:
    HWND m_hwnd;
    char current_key;
};