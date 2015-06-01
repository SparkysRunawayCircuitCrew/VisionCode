
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

void Camera::capture(std::vector<FilterGroup> groups) {
    this->capture_cropped(groups, cv::Rect(0, 0, this->width, this->height));
}

void Camera::capture_cropped(std::vector<FilterGroup> groups, cv::Rect crop_area) {
    this->videoCap >> this->rawFrame;
    this->rawFrame = this->rawFrame(crop_area);

    this->filteredFrame = this->rawFrame;

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
