#pragma once
#include <irrlicht.h>
#include "GameObject.h"
#include "Physics.h"

using namespace irr;
using namespace core;
using namespace scene;
using namespace video;

enum class PickupType { AMMO };

class Pickup : public GameObject
{
public:
	Pickup(ISceneManager* smgr, IVideoDriver* driver, Physics* physics,
		const vector3df& position, PickupType type);
	~Pickup();

	void update(f32 deltaTime) override;

	void collect();
	bool isCollected() const { return m_collected; }
	PickupType getType() const { return m_type; }
	btGhostObject* getTrigger() const { return m_trigger; }

private:
	Physics* m_physics;
	PickupType m_type;

	btGhostObject* m_trigger;
	btSphereShape* m_triggerShape;

	bool m_collected;
	f32 m_respawnTimer;
	vector3df m_spawnPos;
};
