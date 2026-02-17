#pragma once
#include <irrlicht.h>
#include <irrKlang.h>
#include "GameObject.h"
#include "InputHandler.h"
#include "Physics.h"

using namespace irr;
using namespace core;
using namespace scene;
using namespace video;

class Player : public GameObject
{
public:
	Player(ISceneManager* smgr, IVideoDriver* driver, Physics* physics);
	~Player();

	void update(f32 deltaTime) override;
	void handleInput(f32 deltaTime, InputHandler& input, f32 cameraYaw);

	void reset();
	void takeDamage(s32 amount);
	void addAmmo(s32 amount);

	void setMaxHealth(s32 maxHP);
	s32 getMaxHealth() const { return m_maxHealth; }
	void setSkin(IVideoDriver* driver, const char* texturePath);

	// Powerup activation
	void activateSpeedBoost(f32 duration);
	void activateDamageBoost(f32 duration);
	void activateGodMode(f32 duration);

	bool hasSpeedBoost() const { return m_speedBoost; }
	bool hasDamageBoost() const { return m_damageBoost; }
	bool hasGodMode() const { return m_godMode; }
	f32 getSpeedBoostTimer() const { return m_speedBoostTimer; }
	f32 getDamageBoostTimer() const { return m_damageBoostTimer; }
	f32 getGodModeTimer() const { return m_godModeTimer; }

	f32 getRotationY() const { return m_rotationY; }
	bool isShooting() const { return m_isShooting; }
	bool isDead() const { return m_isDead; }
	s32 getAmmo() const { return m_ammo; }
	s32 getHealth() const { return m_health; }
	btCollisionObject* getLastHitObject() const { return m_lastHitObject; }

	struct DebugRay { vector3df start, end; bool active; f32 timer; };
	const DebugRay& getDebugRay() const { return m_debugRay; }

private:
	void handleMovement(f32 deltaTime, InputHandler& input, f32 cameraYaw);
	void handleShooting(f32 deltaTime, InputHandler& input);
	void updateAnimation(bool moving);

	ISceneManager* m_smgr;
	Physics* m_physics;
	IAnimatedMeshSceneNode* m_playerNode;
	IAnimatedMeshSceneNode* m_weaponNode;

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
	s32 m_maxHealth;
	btCollisionObject* m_lastHitObject;
	DebugRay m_debugRay;

	// Powerup state
	bool m_speedBoost;
	bool m_damageBoost;
	bool m_godMode;
	f32 m_speedBoostTimer;
	f32 m_damageBoostTimer;
	f32 m_godModeTimer;

	// Audio
	irrklang::ISoundEngine* m_soundEngine;
	irrklang::ISound* m_runSound;
	void stopRunSound();
};
