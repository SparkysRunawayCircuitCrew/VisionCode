#pragma once

namespace vision {

enum Found: int {
    None,
    Red,
    Yellow,
};

struct FileData {
    int frameCount; 
    Found found;

    int boxWidth, boxHeight;
    int xMid, yBot;

    int safetyFrameCount;

    int getX() const { return xMid - (boxWidth / 2); }
    int getY() const { return yBot - boxHeight; }
    int getWidth() const { return boxWidth; }
    int getHeight() const { return boxHeight; }
};

}
