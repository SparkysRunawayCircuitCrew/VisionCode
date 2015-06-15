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

    // Crop the image
    _cropped = src(cv::Rect(0, 50, src.cols, src.rows - 50));

    // Convert to XYZ color space
    cvtColor(_cropped, _colorTransformed, cv::COLOR_BGR2HSV);

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

void Filter::writeImages(const string& baseName, const Mat& orig, bool writeOrig) const {
    Mat contoursImg, possibleImg, polygonImg, foundImg;
    _cropped.copyTo(contoursImg);
    _cropped.copyTo(possibleImg);
    _cropped.copyTo(polygonImg);
    _cropped.copyTo(foundImg);

    Scalar contourColor(255, 255, 0);
    Scalar polygonLabelColor(255, 128, 200);

    int n = _contours.size();
    for (int i = 0; i < n; i++) {
	drawContours(contoursImg, _contours, i, contourColor, 1);

	vector<Point> polygon;
	Rect br;
        approxPolyDP(Mat(_contours[i]), polygon, 6, true);

	if (isPossibleStanchion(polygon, br)) {
	    drawContours(possibleImg, _contours, i, contourColor, 1);

	    //	    polylines(polygonImg, polygon, false, contourColor, 1);
	    const cv::Point* pts = (const cv::Point*) Mat(polygon).data;
	    int npts = Mat(polygon).rows;
	    polylines(polygonImg, &pts, &npts, 1, true, contourColor, 3);

	    char buf[100];
	    snprintf(buf, sizeof(buf), "%d", polygon.size());
	    Point textPt(br.x + br.width / 2 - 8, br.y + (br.height / 2) - 5);
	    putText(polygonImg, buf, textPt, FONT_HERSHEY_PLAIN,
		    0.75, polygonLabelColor);
	}
    }

    string foundExt;
    bool found = false;

    if (_fileData.found == Found::Red) {
	found = true;
	foundExt = "-red.png";
    } else if (_fileData.found == Found::Yellow) {
	found = true;
	foundExt = "-yellow.png";
    }

    if (found) {
	int x = _fileData.getX();
	int y = _fileData.getY();
	int w = _fileData.getWidth();
	int h = _fileData.getHeight();
	int cx = _fileData.xMid;
	int cy = y + h / 2;
	int rx = x + w;
	int by = _fileData.yBot;
	int iw = foundImg.cols;
	int ih = foundImg.rows;

	Rect br(x, y, w, h);
	rectangle(foundImg, br, contourColor, 3);
	
	int fontSpacing = 15;
	int topText = y - 4;
	int botText = by + fontSpacing;
	int topGap = topText - fontSpacing;
	int botGap = ih - botText;

	int textY = (botGap > topGap) ? botText : topText;
	textY = min(max(fontSpacing, textY), ih - fontSpacing);

	char buf[1024];
	snprintf(buf, sizeof(buf), "%s sz(%dx%d) tl(%d,%d), cp(%d,%d), br(%d,%d)",
		 (_fileData.found == Found::Yellow ? "Yel" : "Red"),
		 w, h, x, y, cx, cy, rx, by);

	putText(foundImg, buf, Point(4, textY), FONT_HERSHEY_PLAIN,
		0.75, contourColor);
    }

    const string names[] = {
	(writeOrig ? "-orig.png" : ""), "-cropped.png",
	"-hsv.png", "-hsv-reduced.png", "-bw.png",
	"-contours.png", "-contours-possible.png", "-polygons.png",
	foundExt
    };
    const Mat* images[] = {
	&orig, &_cropped, &_colorTransformed, &_colorReduced, &_bw,
	&contoursImg, &possibleImg, &polygonImg,
	&foundImg
    };

    n = sizeof(names) / sizeof(names[0]);

    for (int i = 0; i < n; i++) {
	if ((names[i].size() != 0) && (images[i] != 0)) {
	    string fn(baseName);
	    fn += names[i];
	    imwrite(fn, *images[i]);
	}
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
    vector<Vec4i> hierarchy;

    cv::Mat bwClone = _bw.clone();
    findContours(bwClone, _contours, hierarchy, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_SIMPLE);

    int n = _contours.size();
    int maxH = 0;

    for (int i = 0; i < n; i++) {
        vector<Point> polygon;
        Rect br;

        approxPolyDP(Mat(_contours[i]), polygon, 6, true);

        if (isPossibleStanchion(polygon, br) && (br.height > maxH)) {
	    int h = br.height;
	    int w = br.width;
            maxH = h;
            _fileData.boxWidth = w;
            _fileData.boxHeight = h;
            _fileData.xMid = br.x + (w / 2);
            _fileData.yBot = br.y + h;
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
        filter.writeImages(baseName, orig, false);

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

    // Get initial frame and toss (incase first one is bad)
    videoFeed >> origFrame;

    avc::Timer timer;

    int foundLast = -1;

    while (!isInterrupted) {
        videoFeed >> origFrame;

        int found = filter.filter(origFrame);
	if ((found != foundLast) || opts.verbose()) {
	    filter.printFrameRate(cout, timer.secsElapsed());
	    foundLast = found;
	}
    }

    if (filter.getFileData().frameCount > 0) {
	filter.printFrameRate(cout, timer.secsElapsed());

	filter.writeImages(opts.getOutputDir() + "/avc-vision", origFrame, true);
    } else {
	cout << "***ERROR*** Failed to read/process any video frames from camera\n";
    }

    return 0;
}

#endif
