
#include <iostream>
#include <fstream>

#include "camera.hpp"
#include "Timer.h"
#include "color.hpp"
#include "filedata.hpp"

#define YELLOW_RANGE_WINDOW "YellowRange - VisionCode"
#define RED_RANGE_WINDOW "RedRange - VisionCode"

#define COLOR_SHIFT_WINDOW "ColorShift - VisionCode"

#define MIN_HEIGHT 0.2

using namespace vision;

enum Groups: FilterGroup {
    Color,
    Clean,

    YellowRange,
    RedRange,
};

void mouseCallback(int event, int x, int y, int flags, void* data) {
    if (event != cv::EVENT_LBUTTONDOWN) {
        return;
    }

    cv::Mat* image = reinterpret_cast<cv::Mat*>(data);
    cv::Vec3b color = image->at<cv::Vec3b>(cv::Point(x, y));

    std::cout << "(" << x << ", " << y << ")\n";
    //std::cout << color << "\n\n";
}

int largestContour(std::vector<std::vector<cv::Point>>& contours, cv::Rect& boundingRect) {
    int max_contour = 0;
    int max_area = 0;

    for (int i = 0; i < contours.size(); i++) {
        std::vector<cv::Point> polygon;

        cv::approxPolyDP(cv::Mat(contours[i]), polygon, 6, true);

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

void prepareGui(const std::string& windowName, int* xLow, int* xHigh, int* yLow, int* yHigh, int* zLow, int* zHigh) {
    cv::createTrackbar("X Low", windowName, xLow, 255, nullptr, nullptr);
    cv::createTrackbar("Y Low", windowName, yLow, 255, nullptr, nullptr);
    cv::createTrackbar("Z Low", windowName, zLow, 255, nullptr, nullptr);

    cv::createTrackbar("X High", windowName, xHigh, 255, nullptr, nullptr);
    cv::createTrackbar("Y High", windowName, yHigh, 255, nullptr, nullptr);
    cv::createTrackbar("Z High", windowName, zHigh, 255, nullptr, nullptr);
}

void writeData(std::ofstream& file, int frameCount, Found found, cv::Rect& boundingRect) {
    FileData data = {
        frameCount,
        found,

        boundingRect.width,
        boundingRect.height,

        boundingRect.x + (boundingRect.width / 2),
        boundingRect.y,

        frameCount,
    };

    file.seekp(0);
    file.write((const char*)&data, sizeof(data));
    file.flush();
}

int mainOrig(int argc, char* argv[]) {
    cv::Mat colorShift, redRange, yellowRange;

    Camera cam;
    avc::Timer timer;

    std::ofstream file("/dev/shm/stanchions");

    cv::namedWindow(YELLOW_RANGE_WINDOW, 1);
    cv::namedWindow(RED_RANGE_WINDOW, 1);
    cv::namedWindow(COLOR_SHIFT_WINDOW, 1);

    cv::setMouseCallback(COLOR_SHIFT_WINDOW, mouseCallback, &colorShift);

    std::ifstream values("values.txt");

    int redXLow = 0, redXHigh = 0;
    int redYLow = 0, redYHigh = 0;
    int redZLow = 0, redZHigh = 0;

    values >> redXLow; values >> redXHigh;
    values >> redYLow, values >> redYHigh;
    values >> redZLow; values >> redZHigh;

    prepareGui(RED_RANGE_WINDOW, &redXLow, &redXHigh, &redYLow, &redYHigh, &redZLow, &redZHigh);

    int yellowXLow = 0, yellowXHigh = 0;
    int yellowYLow = 0, yellowYHigh = 0;
    int yellowZLow = 0, yellowZHigh = 0;

    values >> yellowXLow; values >> yellowXHigh;
    values >> yellowYLow, values >> yellowYHigh;
    values >> yellowZLow; values >> yellowZHigh;

    prepareGui(YELLOW_RANGE_WINDOW, &yellowXLow, &yellowXHigh, &yellowYLow, &yellowYHigh, &yellowZLow, &yellowZHigh);

    cam.addFilter(Groups::Color, [](cv::Mat& src) { 
        cv::cvtColor(src, src, cv::COLOR_BGR2XYZ);
    });

    cam.addFilter(Groups::RedRange, [&](cv::Mat& src) {
        cv::inRange(src, cv::Scalar(redXLow, redYLow, redZLow), cv::Scalar(redXHigh, redYHigh, redYHigh), src);
    });

    cam.addFilter(Groups::YellowRange, [&](cv::Mat& src) {
        cv::inRange(src, cv::Scalar(yellowXLow, yellowYLow, yellowZLow), cv::Scalar(yellowXHigh, yellowYHigh, yellowYHigh), src);
    });

    float totalDT = 0;
    float totalFPS = 0;
    int frameCount = 0;

    cv::Mat test = cv::imread("webcam-test/red/north/2ftred.png");

    while(true) {
        timer.start();

        colorShift = cam.capture({Groups::Color}, true, &test).clone();

        redRange = cam.capture({Groups::RedRange}, true, &colorShift).clone();
        cv::Rect redBounding = boundingRect(cam.filteredFrame); 
        cv::rectangle(colorShift, redBounding, cv::Scalar(0, 0, 255), 2);

        yellowRange = cam.capture({Groups::YellowRange}, true, &colorShift).clone();
        cv::Rect yellowBounding = boundingRect(cam.filteredFrame); 
        cv::rectangle(colorShift, yellowBounding, cv::Scalar(0, 255, 255), 2);
    
        float redRatio = (float)redBounding.height / cam.height;
        float yellowRatio = (float)yellowBounding.height / cam.height;
        
        Found found = Found::None;
        cv::Rect foundRect;

        if (yellowRatio > MIN_HEIGHT) {
            found = Found::Yellow; 
            foundRect = redBounding;
        } else if (redRatio > MIN_HEIGHT) {
            found = Found::Red;
            foundRect = yellowBounding;
        } else {
            found = Found::None;
            foundRect = cv::Rect(0, 0, 0, 0);
        }

        //std::cout << foundRect << "\n";
        writeData(file, frameCount, found, foundRect);

        cv::imshow(RED_RANGE_WINDOW, redRange);
        cv::imshow(YELLOW_RANGE_WINDOW, yellowRange);
        cv::imshow(COLOR_SHIFT_WINDOW, colorShift);
        
        if ((cv::waitKey(33) & 0xff) == 27) {
            break;
        }

        float delta = timer.secsElapsed();
        float fps = 1.0 / delta;

        totalDT += delta;
        totalFPS += fps;
        frameCount++;
    }

    std::cout << color::Modifier(color::FG_DEFAULT) 
              << "\n" << "Average DT: " << (totalDT / frameCount) << "\tAverage FPS: " << (totalFPS / frameCount) << "\n";
    return 0;
}
