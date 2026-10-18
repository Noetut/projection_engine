#ifndef GEOMETRY_H
#define GEOMETRY_H

#include <windows.h>

struct Point2i {
    int x;
    int y;
};

// Corner ordering is fixed and load-bearing: GDI Polygon(), the calibration
// handles and the JSON schema all assume this clockwise sequence.
enum class Corner {
    TopLeft     = 0,
    TopRight    = 1,
    BottomRight = 2,
    BottomLeft  = 3,
    Count       = 4
};

// A projection area is an arbitrary quadrilateral, not an axis-aligned box: a
// projector mounted off-axis turns physical rectangles into trapezoids, so each
// corner has to move independently during calibration.
struct Quad {
    Point2i corners[4];

    static Quad FromRect(int x, int y, int width, int height) {
        Quad q;
        q.corners[0] = { x,         y          }; // TopLeft
        q.corners[1] = { x + width, y          }; // TopRight
        q.corners[2] = { x + width, y + height }; // BottomRight
        q.corners[3] = { x,         y + height }; // BottomLeft
        return q;
    }

    RECT BoundingBox() const {
        int minX = corners[0].x, maxX = corners[0].x;
        int minY = corners[0].y, maxY = corners[0].y;
        for (int i = 1; i < 4; ++i) {
            if (corners[i].x < minX) minX = corners[i].x;
            if (corners[i].x > maxX) maxX = corners[i].x;
            if (corners[i].y < minY) minY = corners[i].y;
            if (corners[i].y > maxY) maxY = corners[i].y;
        }
        RECT r = { minX, minY, maxX, maxY };
        return r;
    }

    int Width() const {
        RECT r = BoundingBox();
        return r.right - r.left;
    }

    int Height() const {
        RECT r = BoundingBox();
        return r.bottom - r.top;
    }

    // Vertex centroid. For an axis-aligned quad this is the rectangle centre,
    // which keeps parity with the centres stored by the v1 config schema.
    float CenterX() const {
        return (corners[0].x + corners[1].x + corners[2].x + corners[3].x) / 4.0f;
    }

    float CenterY() const {
        return (corners[0].y + corners[1].y + corners[2].y + corners[3].y) / 4.0f;
    }

    // True only when the quad can be rendered through FillRect with identical
    // pixel coverage: edges parallel to the axes and a positive extent. GDI's
    // polygon fill convention differs slightly from FillRect at the borders, so
    // uncalibrated areas must keep taking the FillRect path to stay pixel-exact.
    bool IsAxisAlignedRect() const {
        return corners[0].x == corners[3].x &&
               corners[1].x == corners[2].x &&
               corners[0].y == corners[1].y &&
               corners[3].y == corners[2].y &&
               corners[0].x <  corners[1].x &&
               corners[0].y <  corners[3].y;
    }

    void Translate(int dx, int dy) {
        for (int i = 0; i < 4; ++i) {
            corners[i].x += dx;
            corners[i].y += dy;
        }
    }

    void MoveCorner(int cornerIndex, int dx, int dy) {
        if (cornerIndex < 0 || cornerIndex > 3) return;
        corners[cornerIndex].x += dx;
        corners[cornerIndex].y += dy;
    }
};

#endif // GEOMETRY_H
