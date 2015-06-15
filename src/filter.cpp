#include "filter.hpp"
#include "Timer.h"

#include <iostream>
#include <fstream>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define ENABLE_MAIN 1

using namespace cv;
using namespace vision;
using namespace std;
// ---------------------------------------------------------------------
// ---------------------------------------------------------------------

Filter::Filter() :
    _colorTransformed(),
    _colorReduced(),
    _grayScale(),
    _bw(),
    _redEnabled(true)
{
    memset(&_fileData, 0, sizeof(_fileData));
    loadConfig();
}

// ---------------------------------------------------------------------
// ---------------------------------------------------------------------

Filter::~Filter() {
}

// ---------------------------------------------------------------------
// ---------------------------------------------------------------------

namespace {
    istream& readColorRanges(istream& in, int* ranges) {
        for (int i = 0; i < 6; i++) {
            in >> ranges[i];
        }
        return in;
    }
}

// ---------------------------------------------------------------------
// ---------------------------------------------------------------------

void Filter::loadConfig() {
    std::ifstream values("/etc/avc.conf.d/values.txt");
    readColorRanges(values, _redRanges);
    readColorRanges(values, _yelRanges);
    values.close();
}

// ---------------------------------------------------------------------
// ---------------------------------------------------------------------

Found Filter::filter(const Mat& src) {
    _fileData.frameCount++;
    _fileData.found = Found::None;
    _fileData.boxWidth = _fileData.boxHeight = 0;
    _fileData.xMid = _fileData.yBot = 0;

    // Convert to XYZ color space
    cvtColor(src, _colorTransformed, cv::COLOR_BGR2HSV);

    // Try looking for yellow stanchion first
    Found found = filterColorRange(_yelRanges, Found::Yellow);
    if ((found == Found::None) && _redEnabled) {
        // If yellow not found, then try red
        found = filterColorRange(_redRanges, Found::Red);
    }

    // Transfer final values and set safety frame count to match to signal done
    _fileData.found = found;
    _fileData.safetyFrameCount = _fileData.frameCount;

    return found;
}

// ---------------------------------------------------------------------
// ---------------------------------------------------------------------

void Filter::writeImages(const string& baseName) const {
    const string names[] = { "-hsv.jpg", "-hsv-reduced.jpg", "-bw.jpg" };
    const Mat* images[] = { &_colorTransformed, &_colorReduced, &_bw };

    int n = sizeof(names) / sizeof(names[0]);

    for (int i = 0; i < n; i++) {
        string fn(baseName);
        fn += names[i];
        imwrite(fn, *images[i]);
    }

}

// ---------------------------------------------------------------------
// ---------------------------------------------------------------------

Found Filter::filterColorRange(const int* ranges, Found colorToFind) {
    Scalar lower(ranges[0], ranges[2], ranges[4]);
    Scalar upper(ranges[1], ranges[3], ranges[5]);

    // Filter each color channel for specified ranges
    inRange(_colorTransformed, lower, upper, _colorReduced);

    // Take it down to black and white
    threshold(_colorReduced, _bw, 127, 255, THRESH_BINARY);

    // Now go look for stanchion in black and white image
    vector<vector<Point>> contours;
    vector<Vec4i> hierarchy;

    cv::Mat bwClone = _bw.clone();
    findContours(bwClone, contours, hierarchy, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_SIMPLE);

    int n = contours.size();
    int maxH = 0;

    for (int i = 0; i < n; i++) {
        vector<Point> polygon;

        approxPolyDP(Mat(contours[i]), polygon, 6, true);

        Rect curRect = boundingRect(polygon);
        int w = curRect.width;
        int h = curRect.height;

        if ((h > 15) && (h > maxH) && ((h * 100 / w) > 50)) {
            maxH = h;
            _fileData.boxWidth = w;
            _fileData.boxHeight = h;
            _fileData.xMid = curRect.x + (w / 2);
            _fileData.yBot = curRect.y;
            _fileData.found = colorToFind;
        }
    }

    return _fileData.found;
}

// ---------------------------------------------------------------------
// ---------------------------------------------------------------------

ostream& Filter::print(ostream& out) const {

    out << "Filter frames processed: "
      << _fileData.frameCount
      << "  Found: ";
    if (_fileData.found == Found::Red) {
        out << " Red Stanchion\n";
    } else if (_fileData.found == Found::Yellow) {
        out << " Yellow Stanchion\n";
    } else {
        out << " Nothing";
        return out;
    }

    out << "  Width: " << _fileData.boxWidth
        << "  Height: " << _fileData.boxHeight
        << "  X-Mid: " << _fileData.xMid
        << "  Y-Bot: " << _fileData.yBot;

    return out;
}

// ---------------------------------------------------------------------
// ---------------------------------------------------------------------

ostream& Filter::printFrameRate(ostream& out, float secs) const {
    float frames = getFileData().frameCount;
    float fps = (secs > 0) ? (frames / secs) : 0;

    out << "Frame: " << frames << "  total time: "
	<< secs << " secs (" << fps << " FPS), results:\n"
	<< *this << "\n";

    return out;
}

// ---------------------------------------------------------------------
// ---------------------------------------------------------------------

namespace {
    bool isInterrupted = false;

    void interrupted(int sig) {
        isInterrupted = true; 
    }

    /**
     * Helper class to deal with command line argument options.
     */

    class Options {
    public:
	Options(int argc, char** argv) : 
	    ok(true),
	    verboseOut(false),
	    readFromFile(false),
	    inputFile(""),
	    outputDir("/dev/shm") {

	    int opt;
	    while ((opt = getopt(argc, argv, "hvf:o:")) != -1) {
		switch (opt) {
		case 'f':
		    readFromFile = true;
		    inputFile = optarg;
		    break;
		case 'o':
		    outputDir = optarg;
		    break;

		case 'v':
		    verboseOut = true;
		    break;

		case 'h':
		default:
		    ok = false;
		    cerr << "\n"
"Usage:\n"
"\n"
"  avc-vision [-h] [-f FILE_TO_PROCESS] [-o OUTPUT_DIR]\n"
"\n"
"Where:\n"
"\n"
"  -h\n"
"    Displays this help information\n"
"\n"
"  -v\n"
"    Display more verbose output to the console\n"
"\n"
"  -f FILE_TO_PROCESS\n"
"    Reads image from FILE_TO_PROCESS, processes image, writes out\n"
"    multiple image files for each step of processing and displays\n"
"    summary results to the console.\n"
"\n"
"  -o OUTPUT_DIR\n"
"\n"
"    This option indicates that the program should dump information\n"
"    about the last image processed prior to program termination (only\n"
"    used in streaming mode).\n"
"\n";
		}
	    }
	}

	bool verbose() const { return verboseOut; }
	bool isOk() const { return ok; }
	bool isFileMode() const { return readFromFile; }
	const string& getImageFile() const { return inputFile; }

	const string& getOutputDir() const { return outputDir; }

    private:
	bool ok;

	bool verboseOut;

	// Used to indicate that we should process single image from file
	// (-f FILE)
	bool readFromFile;
	string inputFile;

	// Output directory
	string outputDir;
    };
}

#if ENABLE_MAIN

int main(int argc, char* argv[]) {
    signal(SIGINT, interrupted);
    signal(SIGTERM, interrupted);

    // Evalutate/check command line arguments
    Options opts(argc, argv);
    if (opts.isOk() != true) {
	return 1;
    }

    Filter filter;

    // If processing a single file (-f FILE)
    if (opts.isFileMode()) {
        Mat orig = imread(opts.getImageFile());

        // Create base name for output files
        string baseName(opts.getImageFile());
        int pos = baseName.rfind('.');
        if (pos != string::npos) {
            baseName.erase(pos);
        }

        avc::Timer timer;
        Found found = filter.filter(orig);

	filter.printFrameRate(cout, timer.secsElapsed());

        // Write out individual image files
        filter.writeImages(baseName);

        // Return 0 to parent process if we found image (for scripting)
        return (found == Found::None ? 1 : 0);
    }

    // Video processing
    VideoCapture videoFeed(0);
    if (!videoFeed.isOpened()) {
        cerr << "Failed to open camera\n";
        return 1;
    }

    videoFeed.set(CV_CAP_PROP_FRAME_WIDTH, 320);
    videoFeed.set(CV_CAP_PROP_FRAME_HEIGHT, 240);

    Mat origFrame;

    avc::Timer timer;

    int foundLast = -1;

    while (!isInterrupted) {
        videoFeed >> origFrame;
        origFrame = origFrame(cv::Rect(0, 0, origFrame.cols, origFrame.rows - 50));

        int found = filter.filter(origFrame);
	if ((found != foundLast) || opts.verbose()) {
	    filter.printFrameRate(cout, timer.secsElapsed());
	    foundLast = found;
	}
    }

    filter.printFrameRate(cout, timer.secsElapsed());

    cv::imwrite(opts.getOutputDir() + "/avc-vision-orig.jpg", origFrame);
    filter.writeImages(opts.getOutputDir() + "/avc-vision");

    return 0;
}

#endif
