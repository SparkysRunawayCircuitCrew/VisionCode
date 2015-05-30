
#include <iostream>

#include "camera.hpp"

enum Groups: FilterGroup {
    Stanchion,
};

void mouseCallback(int event, int x, int y, int flags, void* data) {
    if (event != cv::EVENT_LBUTTONDOWN) {
        return;
    }
    cv::Mat* image = reinterpret_cast<cv::Mat*>(data);
    std::cout << x << "\t" << y << ", " << image->at<cv::Vec3b>(cv::Point(x, y)) << "\n";
}

std::vector<cv::Point>* largestContour(std::vector<std::vector<cv::Point>> contours) {
    std::vector<cv::Point>* largestContour = nullptr;
    int max_area = 0;

    for (int i = 0; i < contours.size(); i++) {
        std::vector<cv::Point> polygon;

        cv::approxPolyDP(cv::Mat(contours[i]), polygon, 3, true);
        cv::Rect boundingRect = cv::boundingRect(polygon);

        if (boundingRect.area() > max_area) {
            largestContour = &contours[i];
        }
    }

    return largestContour;
}

int main() {
    Camera cam;
    cv::namedWindow("test", 1);
    cv::setMouseCallback("test", mouseCallback, &cam.rawFrame);

    cam.addFilter(Groups::Stanchion, [](cv::Mat& src) {
        cv::cvtColor(src, src, cv::COLOR_BGR2Luv);
    });

    cam.addFilter(Groups::Stanchion, [](cv::Mat& src) {
        cv::inRange(src, cv::Scalar(120, 100, 145), cv::Scalar(255, 121, 190), src);
    });

    while(true) {
        cam.captureCropped({Groups::Stanchion});
        cv::imshow("test", cam.rawFrame);

        if (cv::waitKey(31) >= 0) {
            break;
        }
    }

    return 0;
}
