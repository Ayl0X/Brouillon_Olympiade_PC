#include "Math.h"
#include "Rebond.h"
#include "Gravite.h"
#include <vector>

struct Vec2 {
	float x;
	float y;
};

class Object {
public:
	Vec2 position;
	Vec2 size;

	Object(Vec2 position, Vec2 size)
		: position(position), size(size) {
	}
};

	bool checkCollision(const Object& a, const Object& b)
	{
		return
			a.position.x < b.position.x + b.size.x &&
			a.position.x + a.size.x > b.position.x &&
			a.position.y < b.position.y + b.size.y &&
			a.position.y + a.size.y > b.position.y;
	}

void update(float deltaTime, std::vector<Object>& objects) {
	for (auto& var : objects)
	{

	}
}