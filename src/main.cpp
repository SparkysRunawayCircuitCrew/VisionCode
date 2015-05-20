
#include "camera.hpp"

enum Groups: FilterGroup {
    Main,
};

int main() {
    Camera cam;
    cv::namedWindow("test", 1);

    cam.addFilter(Groups::Main, [](cv::Mat& src) { cv::inRange(src, cv::Scalar(128, 128, 128), cv::Scalar(255, 255, 255), src); });

    while(true) {
        cam.capture(Groups::Main);
        cv::imshow("test", cam.filteredFrame);
        
        if (cv::waitKey(30) >= 0) {
            break;
        }
    }

    return 0;
}
