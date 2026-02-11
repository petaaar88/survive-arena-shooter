#pragma once
#include <irrlicht.h>
#include "InputHandler.h"

using namespace irr;
using namespace core;
using namespace scene;
using namespace video;

class Player
{
public:
	Player(ISceneManager* smgr, IVideoDriver* driver);
	~Player();

	void update(f32 deltaTime, InputHandler& input, f32 cameraYaw);

	void takeDamage(s32 amount);

	vector3df getPosition() const { return m_position; }
	f32 getRotationY() const { return m_rotationY; }
	bool isShooting() const { return m_isShooting; }
	bool isDead() const { return m_isDead; }
	s32 getAmmo() const { return m_ammo; }
	s32 getHealth() const { return m_health; }
	ISceneNode* getNode() const { return m_playerNode; }

private:
	void handleMovement(f32 deltaTime, InputHandler& input, f32 cameraYaw);
	void handleShooting(f32 deltaTime, InputHandler& input);
	void updateAnimation(bool moving);

	ISceneManager* m_smgr;
	IAnimatedMeshSceneNode* m_playerNode;
	IAnimatedMeshSceneNode* m_weaponNode;

	vector3df m_position;
	f32 m_rotationY;
	vector3df m_forward;
	vector3df m_right;

	bool m_isMoving;
	bool m_isShooting;
	bool m_isDead;
	bool m_isInPain;
	f32 m_shootCooldown;
	f32 m_attackAnimTimer;
	f32 m_painTimer;
	s32 m_ammo;
	s32 m_health;
};
