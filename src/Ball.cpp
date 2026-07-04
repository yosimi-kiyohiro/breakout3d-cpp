#include "Ball.hpp"

Ball::Ball(Vector3 pos, Vector3 vel, float r) : position(pos), velocity(vel), radius(r) {}

void Ball::update(float dt) {
    position.x += velocity.x * dt;
    position.y += velocity.y * dt;
    position.z += velocity.z * dt;
}

void Ball::reset(Vector3 pos, Vector3 vel) {
    position = pos;
    velocity = vel;
}
