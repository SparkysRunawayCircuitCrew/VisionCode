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
};

}
