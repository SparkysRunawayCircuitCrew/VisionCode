
#include <iostream>

#include "camera.hpp"
#include <opencv2/opencv.hpp>

using namespace vision;

// we assume each captured frame has the same resolution
Camera::Camera(int id): videoCap(id) { 
    this->prepareVideoCap();
}

Camera::Camera(std::string url): videoCap(url) { 
    this->prepareVideoCap();
}

void Camera::prepareVideoCap() {
    if (!videoCap.isOpened()) {
        std::cout << "Failed to open camera" << "\n";
    }

    this->videoCap.set(CV_CAP_PROP_FRAME_WIDTH, 320);
    this->videoCap.set(CV_CAP_PROP_FRAME_HEIGHT, 240);

    this->videoCap >> this->rawFrame;

    this->width = 320;
    this->height = 240;

    this->filteredFrame = this->rawFrame.clone();
}

cv::Mat& Camera::capture(std::vector<FilterGroup> groups, bool clearPreviousFilters, cv::Mat* image) {
    return this->captureCropped(groups, cv::Rect(0, 0, this->width, this->height), clearPreviousFilters, image);
}

cv::Mat& Camera::captureCropped(std::vector<FilterGroup> groups, cv::Rect cropArea, bool clearPreviousFilters, cv::Mat* image) {
    if (image == nullptr) {
        this->videoCap >> this->rawFrame;
    } else {
        this->rawFrame = image->clone();
    }

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
