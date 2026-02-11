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

	vector3df getPosition() const { return m_position; }
	f32 getRotationY() const { return m_rotationY; }
	bool isShooting() const { return m_isShooting; }
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
	f32 m_shootCooldown;
	f32 m_attackAnimTimer;
};
