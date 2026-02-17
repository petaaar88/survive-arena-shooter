#pragma once
#include <irrlicht.h>
#include "GameObject.h"
#include "Physics.h"

using namespace irr;
using namespace core;
using namespace scene;
using namespace video;

enum class PowerupType { SPEED_BOOST, DAMAGE_BOOST, GOD_MODE };

// Durations (seconds) for each powerup
static const f32 SPEED_BOOST_DURATION = 10.0f;
static const f32 DAMAGE_BOOST_DURATION = 8.0f;
static const f32 GOD_MODE_DURATION = 5.0f;

class Powerup : public GameObject
{
public:
	Powerup(ISceneManager* smgr, IVideoDriver* driver, Physics* physics,
		const vector3df& position, PowerupType type);
	~Powerup();

	void update(f32 deltaTime) override;

	void collect();
	bool isCollected() const { return m_collected; }
	bool isExpired() const { return m_expired; }
	PowerupType getType() const { return m_type; }
	btGhostObject* getTrigger() const { return m_trigger; }

	f32 getDuration() const;

	void setLifetime(f32 seconds);
	f32 getLifetimeRemaining() const { return m_lifetime; }

private:
	Physics* m_physics;
	PowerupType m_type;

	btGhostObject* m_trigger;
	btSphereShape* m_triggerShape;

	bool m_collected;
	bool m_expired;
	f32 m_lifetime;
	vector3df m_spawnPos;
};
