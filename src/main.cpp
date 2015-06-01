
#include <iostream>

#include "camera.hpp"

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

void drawBoundingRect(cv::Mat& frame, cv::Mat& canvas) {
    std::vector<std::vector<cv::Point>> contours;
    std::vector<cv::Vec4i> hierarchy;

    cv::findContours(frame, contours, hierarchy, CV_RETR_LIST, CV_CHAIN_APPROX_NONE, cv::Point(0, 0));

    cv::Rect boundingRect;
    largestContour(contours, boundingRect); 

    cv::rectangle(canvas, boundingRect, cv::Scalar(0, 0, 255), 2);
}

int main() {
    Camera cam;
    cv::namedWindow("test", 1);
    cv::setMouseCallback("test", mouseCallback, &cam.filteredFrame);

    cam.addFilter(Groups::Color, [](cv::Mat& src) { 
        cv::cvtColor(src, src, cv::COLOR_BGR2HSV);
    });

    cam.addFilter(Groups::Range, [](cv::Mat& src) {
        cv::inRange(src, cv::Scalar(10, 130, 70), cv::Scalar(40, 230, 200), src);
    });

    while(true) {
        std::vector<FilterGroup> filters;
        filters.push_back(Groups::Color);
        filters.push_back(Groups::Range);

        cam.capture(filters);
        
        drawBoundingRect(cam.filteredFrame, cam.rawFrame); 
        cv::imshow("test", cam.rawFrame);
        
        if (cv::waitKey(31) >= 0) {
            break;
        }
    }

    return 0;
}
