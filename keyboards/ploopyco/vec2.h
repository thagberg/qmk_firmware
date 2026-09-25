#include <stdbool.h>
#include <math.h>

struct Vec2 {
   float x;
   float y;
};
struct Vec2 previous_pvector = {0.f, 0.f};
struct Vec2 previous_pnormal_left = {0.f, 0.f};
struct Vec2 previous_pnormal_right = {0.f, 0.f};

float v2_length(const struct Vec2* v) {
    return sqrtf(v->x*v->x + v->y*v->y);
}

struct Vec2 v2_scalar_div(const struct Vec2* v, float s) {
    return (struct Vec2){v->x / s, v->y / s};
}

struct Vec2 v2_scalar_mul(const struct Vec2* v, float s) {
    return (struct Vec2){v->x * s, v->y * s};
}

struct Vec2 v2_normalize(const struct Vec2* p) {
    if (p == NULL) {
        return (struct Vec2){0.f, 0.f};
    }

    float l = v2_length(p);
    return v2_scalar_div(p, l);
}

float v2_dot(const struct Vec2* l, const struct Vec2* r) {
    return l->x * r-> x + l->y * r->y;
}
 float v2_cross(const struct Vec2* l, const struct Vec2* r) {
     return (l->x * r->y) - (l->y * r->x);
 }

struct Vec2 v2_add(const struct Vec2*l, const struct Vec2* r) {
    return (struct Vec2){l->x + r->x, l->y + r->y};
}

struct Vec2 v2_sub(const struct Vec2*l, const struct Vec2* r) {
    return (struct Vec2){l->x - r->x, l->y - r->y};
}

float v2_cos_theta(const struct Vec2* l, const struct Vec2* r) {
    struct Vec2 nl = v2_normalize(l);
    struct Vec2 nr = v2_normalize(r);

    return v2_dot(&nl, &nr);
}

float v2_project(const struct Vec2* v, const struct Vec2* n) {
    // this is probably the most expensive way of calculating this and
    // it should definitely be optimized
    float cos_theta = v2_cos_theta(v, n);
    return cos_theta * v2_length(v);
}

float v2_angle(const struct Vec2* a, const struct Vec2* b) {
    return atan2f(v2_cross(a, b), v2_dot(a, b));
}

