#include "Block.hpp"
#include "constants.hpp"

Block::Block(Vector3 pos, float halfWidth_, float halfHeight_, int durability_)
    : position(pos),
      halfWidth(halfWidth_),
      halfHeight(halfHeight_),
      halfDepth(Constants::BLOCK_HALF_DEPTH),
      durability(durability_),
      initialDurability(durability_),
      alive(true) {}

BoundingBox Block::getBoundingBox() const {
    BoundingBox box;
    box.min = {position.x - halfWidth, position.y - halfHeight, position.z - halfDepth};
    box.max = {position.x + halfWidth, position.y + halfHeight, position.z + halfDepth};
    return box;
}

void Block::hit() {
    durability -= 1;
    if (durability <= 0) {
        alive = false;
    }
}
