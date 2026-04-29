#pragma once
#include <opencv2/opencv.hpp>
#include <string>
#include <filesystem>
#include <iostream>

class Template {
public:
    // 构造函数：文件名，以及该 UI 在 1280x720 下的坐标 (x1, y1, x2, y2)
    Template(const std::string& filename, int x1, int y1, int x2, int y2) {
        int w = x2 - x1;
        int h = y2 - y1;
        m_tplImage = cv::imread(filename, cv::IMREAD_COLOR);
        m_roiRect = cv::Rect(x1, y1, w, h);

        if (m_tplImage.empty()) {
            std::cerr << "[错误] 模板加载失败: " << filename << std::endl;
        }
        else {
            // 确保模板图被缩放到 ROI 指定的大小，防止图片素材尺寸不一致
            cv::resize(m_tplImage, m_tplImage, cv::Size(w, h));
            m_basename = std::filesystem::path(filename).filename().string();
            std::cout << "[模板] 已加载 " << filename << "，预设位置: ("
                << x1 << ", " << y1 << ") " << w << "x" << h << std::endl;
        }
    }

    void setIsDebug(bool debug) {
        is_debug = debug;
	}

    void setDefaultKey(char defaultKey) {
        m_defaultKey = defaultKey;
	}

    int getCenterX() const {
        return m_roiRect.x + m_roiRect.width / 2;
	}

    int getCenterY() const {
        return m_roiRect.y + m_roiRect.height / 2;
	}

    std::string getTplName() const {
        return m_basename;
	}

    char getDefaultKey() const { return m_defaultKey; }

    /**
     * 将当前截图的 ROI 区域保存到本地，用于调试排查
     */
    void saveDebugImg(const cv::Mat& screenshot) const {
        if (screenshot.empty()) return;
        // 检查文件是否已存在
        std::string outName = "debug_" + m_basename;
        //if (std::filesystem::exists(outName)) {
        //    //std::cout << "[] 文件已存在，跳过保存: " << outName << std::endl;
        //    return;
        //}
        cv::Mat roi = screenshot(m_roiRect).clone();
        cv::imwrite(outName, roi);
        std::cout << "[调试] ROI 已保存至: " << outName << std::endl;
        exit(0);
    }

    /**
     * 比较指定坐标区域是否匹配
     */
    bool match(const cv::Mat& screenshot, double threshold = 0.8) const {
        auto t1 = std::chrono::high_resolution_clock::now();

        if (screenshot.empty() || m_tplImage.empty()) return false;

        // 1. 裁剪当前画面的指定区域
        // 注意：由于 WGC 截图可能是 BGRA，这里统一转为 BGR 确保和 imread 的模板一致
        cv::Mat screenBGR;
        if (screenshot.channels() == 4) {
            cv::cvtColor(screenshot, screenBGR, cv::COLOR_BGRA2BGR);
        }
        else {
            screenBGR = screenshot;
        }

        // 2. 提取 ROI
        cv::Mat roi = screenBGR(m_roiRect);

        // 3. 模板匹配 (使用 TM_CCOEFF_NORMED)
        cv::Mat res;
        cv::matchTemplate(roi, m_tplImage, res, cv::TM_CCOEFF_NORMED);

        double maxVal;
        cv::minMaxLoc(res, nullptr, &maxVal);

        // 如果需要调试，可以取消下面这行注释
        if (is_debug) {
            auto t2 = std::chrono::high_resolution_clock::now();
            auto det_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
            std::cout << "  -> [" << m_basename << "] 相似度: " << maxVal << " | 识别耗时: " << det_ms << "ms" << std::endl;
        }

        if (maxVal >= threshold) {
            return true;
        }

        if (is_debug) {
            cv::imwrite("Debug_" + m_basename, roi);
            cv::imwrite("Debug_screenshot.png", screenBGR);
        }
          
        return false;
    }

    
    bool matchAndGetRect(const cv::Mat& screenshot, cv::Rect& outRect, double threshold = 0.8) const {
        if (screenshot.empty() || m_tplImage.empty()) return false;

        // 1. 颜色空间转换 (保持与模板一致)
        cv::Mat screenBGR;
        if (screenshot.channels() == 4) {
            cv::cvtColor(screenshot, screenBGR, cv::COLOR_BGRA2BGR);
        }
        else {
            screenBGR = screenshot;
        }

        // 安全检查：确保 ROI 区域没有超出截图边界
        cv::Rect safeRoi = m_roiRect & cv::Rect(0, 0, screenBGR.cols, screenBGR.rows);
        if (safeRoi.width <= 0 || safeRoi.height <= 0) return false;

        // 2. 提取 ROI
        cv::Mat roi = screenBGR(safeRoi);

        // 3. 模板匹配
        cv::Mat res, grayRoi, grayTpl;
        //cv::matchTemplate(roi, m_tplImage, res, cv::TM_CCOEFF_NORMED);
        cv::cvtColor(roi, grayRoi, cv::COLOR_BGR2GRAY);
        cv::cvtColor(m_tplImage, grayTpl, cv::COLOR_BGR2GRAY);
        cv::matchTemplate(grayRoi, grayTpl, res, cv::TM_CCOEFF_NORMED);

        double maxVal;
        cv::Point maxLoc; // 用于记录匹配到的左上角点（ROI 相对坐标）
        cv::minMaxLoc(res, nullptr, &maxVal, nullptr, &maxLoc);

        if (is_debug) {
            std::cout << "  -> [" << m_basename << "] 相似度: " << maxVal
                << " 位置: " << maxLoc << std::endl;
            cv::imwrite("debug_roi.png", roi);
            cv::imwrite("debug_tpl.png", m_tplImage);
        }

        // 4. 判断阈值并计算绝对坐标
        if (maxVal >= threshold) {
            // 计算在原始全屏截图中的位置：
            // 绝对 X = ROI起始X + 相对匹配点X
            // 绝对 Y = ROI起始Y + 相对匹配点Y
            outRect = cv::Rect(
                safeRoi.x + maxLoc.x,
                safeRoi.y + maxLoc.y,
                m_tplImage.cols,
                m_tplImage.rows
            );
            return true;
        }

        return false;
    }

    bool matchAndGetRect1(const cv::Mat& screenshot, cv::Rect& outRect) const {
        if (screenshot.empty() || m_tplImage.empty()) return false;

        // 1. 颜色处理：转为灰度图（减少颜色微调带来的干扰）
        cv::Mat screenGray, tplGray;
        if (screenshot.channels() == 4) {
            cv::Mat temp;
            cv::cvtColor(screenshot, temp, cv::COLOR_BGRA2BGR);
            cv::cvtColor(temp, screenGray, cv::COLOR_BGR2GRAY);
        }
        else {
            cv::cvtColor(screenshot, screenGray, cv::COLOR_BGR2GRAY);
        }
        cv::cvtColor(m_tplImage, tplGray, cv::COLOR_BGR2GRAY);

        // 2. 提取 ROI (此时用灰度图提取)
        cv::Rect safeRoi = m_roiRect & cv::Rect(0, 0, screenGray.cols, screenGray.rows);
        if (safeRoi.width < tplGray.cols || safeRoi.height < tplGray.rows) return false;
        cv::Mat roiGray = screenGray(safeRoi);

        // 强制检查：在 matchTemplate 前保存一张图，看颜色正不正
        cv::imwrite("CHECK_ROI_COLOR.bmp", roiGray);
        cv::imwrite("CHECK_TPL_COLOR.bmp", tplGray);

        // 3. 使用模板匹配 (它能处理这种几个像素的位移)
        cv::Mat result;
        cv::matchTemplate(roiGray, tplGray, result, cv::TM_CCOEFF_NORMED);

        double maxVal;
        cv::Point maxLoc;
        cv::minMaxLoc(result, nullptr, &maxVal, nullptr, &maxLoc);

        if (is_debug) {
            std::cout << "[调试] " << m_basename << " 最高相似度: " << maxVal << std::endl;
        }

        // 4. 这里是重点：阈值设为 0.7 左右
        // 因为你的 REAL_DIFF 显示轮廓很准，说明形状没变，0.7 足够过滤掉错误的格子
        if (maxVal >= 0.7) {
            outRect = cv::Rect(
                safeRoi.x + maxLoc.x,
                safeRoi.y + maxLoc.y,
                m_tplImage.cols,
                m_tplImage.rows
            );
            return true;
        }

        return false;
    }

private:
    std::string m_filename;
    std::string m_basename;
    cv::Mat m_tplImage;
    cv::Rect m_roiRect;
    char m_defaultKey;
    bool is_debug;
};