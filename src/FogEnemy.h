#pragma once
#include <irrlicht.h>
#include <irrklang.h>
#include "GameObject.h"
#include "Physics.h"

using namespace irr;
using namespace core;
using namespace scene;
using namespace video;

enum class FogEnemyState { SPAWNING, FALLBACK, REPOSITION, THROWING, IDLE, DEAD };

class FogEnemy : public GameObject
{
public:
	FogEnemy(ISceneManager* smgr, IVideoDriver* driver, Physics* physics,
		const vector3df& spawnPos, const vector3df& forward,
		irrklang::ISoundEngine* soundEngine = nullptr);
	~FogEnemy();

	void update(f32 deltaTime) override;
	void updateAI(f32 deltaTime, const vector3df& playerPos);

	bool isDead() const { return m_isDead; }
	bool isFogActive() const { return m_fogActive; }
	void takeDamage(s32 amount);

	FogEnemyState getState() const { return m_state; }

private:
	void createPhysicsBody(const vector3df& pos);
	void spawnGrenade();
	void updateGrenade(f32 deltaTime);
	void activateFog();
	void updateFog(f32 deltaTime);

	ISceneManager* m_smgr;
	IVideoDriver* m_driver;
	Physics* m_physics;
	irrklang::ISoundEngine* m_soundEngine;
	IAnimatedMeshSceneNode* m_animNode;

	f32 m_rotationY;

	FogEnemyState m_state;
	bool m_isDead;
	bool m_isInPain;

	s32 m_health;
	f32 m_stateTimer;
	f32 m_deathTimer;
	f32 m_painTimer;

	// Gate spawn
	vector3df m_spawnForward;
	f32 m_spawnWalkDistance;
	f32 m_spawnDistanceTraveled;
	bool m_physicsCreated;

	// Grenade
	bool m_grenadeActive;
	ISceneNode* m_grenadeNode;
	vector3df m_grenadeVelocity;

	// Stuck detection & obstacle avoidance
	vector3df m_lastCheckedPos;
	f32       m_stuckTimer;
	bool      m_isStrafing;
	f32       m_strafeTimer;
	f32       m_strafeDirection; // +1 = right, -1 = left

	// Fog (Irrlicht EFT_FOG_LINEAR)
	bool m_fogActive;
	f32 m_fogTimer;
	f32 m_fogStartDist;
	f32 m_fogEndDist;
	bool m_fogFinished;

	int m_currentRepositionIndex;
	f32 m_movementSpeed;
};
