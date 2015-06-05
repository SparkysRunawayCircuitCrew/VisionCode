
#include <iostream>

#include "camera.hpp"
#include "Timer.h"
#include "color.hpp"

using namespace vision;

enum Groups: FilterGroup {
    Color,
    Range,
};

void mouseCallback(int event, int x, int y, int flags, void* data) {
    if (event != cv::EVENT_LBUTTONDOWN) {
        return;
    }

    cv::Mat* image = reinterpret_cast<cv::Mat*>(data);
    std::cout << x << "\t" << y << ", " << image->at<cv::Vec3b>(cv::Point(x, y)) << "\n";
}

int largestContour(std::vector<std::vector<cv::Point>>& contours, cv::Rect& boundingRect) {
    int max_contour = 0;
    int max_area = 0;

    for (int i = 0; i < contours.size(); i++) {
        std::vector<cv::Point> polygon;

        cv::approxPolyDP(cv::Mat(contours[i]), polygon, 3, true);
        cv::Rect curRect = cv::boundingRect(polygon);

        if (curRect.area() > max_area) {
            max_area = curRect.area();
            boundingRect = curRect;

            max_contour = i;
        }
    } 

    return max_contour;
}

cv::Rect boundingRect(cv::Mat& frame) {
    std::vector<std::vector<cv::Point>> contours;
    std::vector<cv::Vec4i> hierarchy;

    cv::findContours(frame, contours, hierarchy, CV_RETR_LIST, CV_CHAIN_APPROX_NONE, cv::Point(0, 0));

    cv::Rect boundingRect;
    largestContour(contours, boundingRect); 

    return boundingRect;
}

int main() {
    cv::Mat colorShift;
    cv::Mat inRange;

    Camera cam;
    cv::namedWindow("test", 1);
    cv::setMouseCallback("test", mouseCallback, &colorShift);

    cam.addFilter(Groups::Color, [](cv::Mat& src) { 
        cv::cvtColor(src, src, cv::COLOR_BGR2HSV);
    });

    cam.addFilter(Groups::Range, [](cv::Mat& src) {
        cv::inRange(src, cv::Scalar(10, 125, 40), cv::Scalar(20, 210, 140), src);
    });

    avc::Timer timer;

    float totalDT = 0;
    float totalFPS = 0;
    int frameCount = 0;

    while(true) {
        timer.start();

        colorShift = cam.capture({Groups::Color}).clone();
        inRange = cam.capture({Groups::Range}, false).clone();
        
        cv::Rect bounding = boundingRect(inRange); 
        cv::rectangle(cam.rawFrame, bounding, cv::Scalar(0, 0, 255), 2);
        cv::imshow("test", cam.rawFrame);
        
        if (cv::waitKey(33) >= 0) {
            break;
        }

        float delta = timer.secsElapsed();
        float fps = 1.0 / delta;

        totalDT += delta;
        totalFPS += fps;
        frameCount++;

        color::Modifier col( (fps < 7) ? color::FG_RED : color::FG_GREEN);
        std::cout << col << "DT: " << delta << "\tFPS: " << fps << "\n";
    }

    std::cout << color::Modifier(color::FG_DEFAULT) 
              << "\n" << "Average DT: " << (totalDT / frameCount) << "\tAverage FPS: " << (totalFPS / frameCount) << "\n";

    return 0;
}
