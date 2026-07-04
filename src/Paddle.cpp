#include "Paddle.hpp"
#include "constants.hpp"
#include "raymath.h"

Paddle::Paddle(Vector3 pos, float halfWidth_, float halfHeight_, float halfDepth_)
    : position(pos), halfWidth(halfWidth_), halfHeight(halfHeight_), halfDepth(halfDepth_) {}

BoundingBox Paddle::getBoundingBox() const {
    BoundingBox box;
    box.min = {position.x - halfWidth, position.y - halfHeight, position.z - halfDepth};
    box.max = {position.x + halfWidth, position.y + halfHeight, position.z + halfDepth};
    return box;
}

void Paddle::moveX(float dx) {
    position.x += dx;
    float minX = Constants::X_MIN + halfWidth;
    float maxX = Constants::X_MAX - halfWidth;
    position.x = Clamp(position.x, minX, maxX);
}
