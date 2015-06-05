
#include <iostream>

#include "camera.hpp"
#include <opencv2/opencv.hpp>

using namespace vision;

// we assume each captured frame has the same resolution
Camera::Camera(int id): videoCap(id) { 
    this->videoCap >> this->rawFrame;

    this->videoCap.set(CV_CAP_PROP_FRAME_WIDTH, 320);
    this->videoCap.set(CV_CAP_PROP_FRAME_HEIGHT, 240);

    this->width = 320;
    this->height = 240;

    this->filteredFrame = this->rawFrame.clone();
}

cv::Mat& Camera::capture() {
    return this->capture({});
}

cv::Mat& Camera::capture(std::vector<FilterGroup> groups, bool clearPreviousFilters) {
    return this->captureCropped(groups, cv::Rect(0, 0, this->width, this->height), clearPreviousFilters);
}

cv::Mat& Camera::captureCropped(std::vector<FilterGroup> groups, cv::Rect cropArea, bool clearPreviousFilters) {
    this->videoCap >> this->rawFrame;
    this->rawFrame = this->rawFrame(cropArea);

    if (clearPreviousFilters) {
        this->filteredFrame = this->rawFrame.clone();
    }

    for (FilterGroup& group : groups) {
        for (auto& filter : this->filters[group]) {
            filter(this->filteredFrame);
        }
    }

    return this->filteredFrame;
}

void Camera::addFilter(FilterGroup group, FilterFunc filter) {
    // Prevents segfault by ensuring vector is large enough
    if (group >= this->filters.size()) {
        this->filters.resize(group + 1);
    }

    this->filters[group].push_back(filter);
}
