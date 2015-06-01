
#include <iostream>

#include "camera.hpp"
#include <opencv2/opencv.hpp>

// we assume each captured frame has the same resolution
Camera::Camera(int id): videoCap(id) { 
    cv::Mat test_frame;
    this->videoCap >> test_frame;

    this->width = test_frame.cols;
    this->height = test_frame.rows;
}

void Camera::capture() {
    this->capture({});
}

void Camera::capture(std::vector<FilterGroup> groups) {
    this->captureCropped(groups, cv::Rect(0, 0, this->width, this->height));
}

void Camera::captureCropped(std::vector<FilterGroup> groups, cv::Rect cropArea) {
    this->videoCap >> this->rawFrame;
    this->rawFrame = this->rawFrame(cropArea);

    this->filteredFrame = this->rawFrame.clone();

    for (FilterGroup& group : groups) {
        for (auto& filter : this->filters[group]) {
            filter(this->filteredFrame);
        }
    }
}

void Camera::addFilter(FilterGroup group, FilterFunc filter) {
    // Prevents segfault by ensuring vector is large enough
    if (group >= this->filters.size()) {
        this->filters.resize(group + 1);
    }

    this->filters[group].push_back(filter);
}
