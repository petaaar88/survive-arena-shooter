#pragma once
#include <irrlicht.h>
#include <irrklang.h>
#include "GameObject.h"
#include "Physics.h"

using namespace irr;
using namespace core;
using namespace scene;
using namespace video;

enum class EnemyState { SPAWNING, SALUTING, IDLE, CHASE, WAIT_ATTACK, ATTACK, DEAD };

class Enemy : public GameObject
{
public:
	Enemy(ISceneManager* smgr, IVideoDriver* driver, Physics* physics, const vector3df& spawnPos,
		  const vector3df& forward = vector3df(0, 0, 0),
		  irrklang::ISoundEngine* soundEngine = nullptr);
	~Enemy();

	void update(f32 deltaTime) override;
	void updateAI(f32 deltaTime, const vector3df& playerPos);

	bool isDead() const { return m_isDead; }

	void takeDamage(s32 amount);

	bool wantsToDealDamage() const;
	s32 getAttackDamage() const;
	void resetAttackCooldown();
	btGhostObject* getAttackTrigger() const { return m_attackTrigger; }

	void setAttackAllowed(bool allowed) { m_attackAllowed = allowed; }
	void setSaluteAllowed(bool allowed) { m_saluteAllowed = allowed; }
	bool isSaluting() const { return m_isSaluting; }
	EnemyState getState() const { return m_state; }

private:
	void updateAttackTrigger();
	void createPhysicsBody(const vector3df& pos);

	ISceneManager* m_smgr;
	IVideoDriver* m_driver;
	Physics* m_physics;
	irrklang::ISoundEngine* m_soundEngine;
	IAnimatedMeshSceneNode* m_animNode;

	f32 m_rotationY;

	EnemyState m_state;
	bool m_isDead;
	bool m_isMoving;
	bool m_isInPain;

	s32 m_health;
	f32 m_attackCooldown;
	f32 m_deathTimer;
	f32 m_painTimer;

	bool m_attackAllowed;

	btGhostObject* m_attackTrigger;
	btSphereShape* m_attackShape;

	// Stuck detection & obstacle avoidance
	vector3df m_lastCheckedPos;
	f32       m_stuckTimer;
	bool      m_isStrafing;
	f32       m_strafeTimer;
	f32       m_strafeDirection; // +1 = right, -1 = left

	// Salute pause (balancing mechanic)
	bool m_isSaluting;
	f32  m_saluteTimer;
	f32  m_saluteCooldown;
	bool m_saluteAllowed;

	// Gate spawn
	vector3df m_spawnForward;
	f32 m_spawnWalkDistance;
	f32 m_spawnDistanceTraveled;
	bool m_physicsCreated;
};
