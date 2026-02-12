#pragma once
#include <irrlicht.h>
#include "GameObject.h"
#include "Physics.h"

using namespace irr;
using namespace core;
using namespace scene;
using namespace video;

enum class EnemyState { IDLE, CHASE, ATTACK, DEAD };

class Enemy : public GameObject
{
public:
	Enemy(ISceneManager* smgr, IVideoDriver* driver, Physics* physics, const vector3df& spawnPos);
	~Enemy();

	void update(f32 deltaTime) override;
	void updateAI(f32 deltaTime, const vector3df& playerPos);

	bool isDead() const { return m_isDead; }

	void takeDamage(s32 amount);

	bool wantsToDealDamage() const;
	s32 getAttackDamage() const;
	void resetAttackCooldown();
	btGhostObject* getAttackTrigger() const { return m_attackTrigger; }

private:
	void updateAttackTrigger();
	ISceneManager* m_smgr;
	Physics* m_physics;
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

	btGhostObject* m_attackTrigger;
	btSphereShape* m_attackShape;
};
