#pragma once
#include <irrlicht.h>

using namespace irr;
using namespace core;
using namespace scene;
using namespace video;

enum class EnemyState { IDLE, CHASE, ATTACK, DEAD };

class Enemy
{
public:
	Enemy(ISceneManager* smgr, IVideoDriver* driver, const vector3df& spawnPos);
	~Enemy();

	void update(f32 deltaTime, const vector3df& playerPos);

	vector3df getPosition() const { return m_position; }
	bool isDead() const { return m_isDead; }
	bool shouldRemove() const { return m_removeMe; }
	ISceneNode* getNode() const { return m_node; }

	void takeDamage(s32 amount);

private:
	ISceneManager* m_smgr;
	IAnimatedMeshSceneNode* m_node;

	vector3df m_position;
	f32 m_rotationY;

	EnemyState m_state;
	bool m_isDead;
	bool m_removeMe;
	bool m_isMoving;

	s32 m_health;
	f32 m_attackCooldown;
	f32 m_deathTimer;
};
