#include "Player.h"
#include <cmath>

static const f32 PLAYER_SPEED = 200.0f;
static const f32 GUN_FIRE_RATE = 0.8f;
static const f32 ATTACK_ANIM_DURATION = 0.5f;
static const f32 SHOOT_RANGE = 1000.0f;
static const f32 MD2_ROTATION_OFFSET = -90.0f;
static const s32 PLAYER_AMMO_START = 115;
static const s32 PLAYER_HEALTH_START = 100;
static const f32 WAVE_ANIM_DURATION = 0.5f;
static const f32 PAIN_ANIM_DURATION = 0.4f;

Player::Player(ISceneManager* smgr, IVideoDriver* driver, Physics* physics)
	: GameObject(nullptr, nullptr)
	, m_smgr(smgr)
	, m_physics(physics)
	, m_playerNode(nullptr)
	, m_weaponNode(nullptr)
	, m_rotationY(0.0f)
	, m_forward(0, 0, 1)
	, m_right(1, 0, 0)
	, m_isMoving(false)
	, m_isShooting(false)
	, m_isDead(false)
	, m_isInPain(false)
	, m_shootCooldown(0.0f)
	, m_attackAnimTimer(0.0f)
	, m_painTimer(0.0f)
	, m_ammo(PLAYER_AMMO_START)
	, m_health(PLAYER_HEALTH_START)
	, m_lastHitObject(nullptr)
	, m_debugRay{ vector3df(0,0,0), vector3df(0,0,0), false, 0.0f }
{
	// Load player model
	IAnimatedMesh* playerMesh = smgr->getMesh("assets/models/player/tris.md2");
	if (playerMesh)
	{
		m_playerNode = smgr->addAnimatedMeshSceneNode(playerMesh);
		if (m_playerNode)
		{
			m_playerNode->setMaterialTexture(0, driver->getTexture("assets/models/player/caleb.pcx"));
			m_playerNode->setMaterialFlag(EMF_LIGHTING, false);
			m_playerNode->setPosition(vector3df(0, 0, 0));
			m_playerNode->setMD2Animation(EMAT_STAND);
		}
	}

	// Load weapon model, attach to player
	IAnimatedMesh* weaponMesh = smgr->getMesh("assets/models/player/weapon.md2");
	if (weaponMesh && m_playerNode)
	{
		m_weaponNode = smgr->addAnimatedMeshSceneNode(weaponMesh, m_playerNode);
		if (m_weaponNode)
		{
			m_weaponNode->setMaterialTexture(0, driver->getTexture("assets/models/player/Weapon.pcx"));
			m_weaponNode->setMaterialFlag(EMF_LIGHTING, false);
			m_weaponNode->setMD2Animation(EMAT_STAND);
		}
	}

	// Create kinematic Bullet capsule
	btCapsuleShape* capsule = new btCapsuleShape(15.0f, 30.0f);
	m_body = physics->createRigidBody(0.0f, capsule, vector3df(0, 0, 0));
	m_body->setCollisionFlags(m_body->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
	m_body->setActivationState(DISABLE_DEACTIVATION);
	m_body->setUserPointer(this);

	m_node = m_playerNode;
}

Player::~Player()
{
	// Irrlicht scene manager owns the nodes — cleaned up on device->drop()
	// Physics destructor cleans up all bodies from world
}

void Player::update(f32 deltaTime)
{
	// Sync physics → node + apply rotation
	syncPhysicsToNode();

	if (m_playerNode)
		m_playerNode->setRotation(vector3df(0, m_rotationY + MD2_ROTATION_OFFSET, 0));
}

void Player::handleInput(f32 deltaTime, InputHandler& input, f32 cameraYaw)
{
	m_lastHitObject = nullptr;

	// Tick debug ray timer
	if (m_debugRay.active)
	{
		m_debugRay.timer -= deltaTime;
		if (m_debugRay.timer <= 0)
			m_debugRay.active = false;
	}

	if (m_isDead)
		return;

	// Tick pain animation
	if (m_isInPain)
	{
		m_painTimer -= deltaTime;
		if (m_painTimer <= 0)
		{
			m_isInPain = false;
			if (m_playerNode)
			{
				m_playerNode->setMD2Animation(m_isMoving ? EMAT_RUN : EMAT_STAND);
				if (m_weaponNode)
					m_weaponNode->setMD2Animation(m_isMoving ? EMAT_RUN : EMAT_STAND);
			}
		}
		else
		{
			return;
		}
	}

	handleMovement(deltaTime, input, cameraYaw);
	handleShooting(deltaTime, input);
}

void Player::takeDamage(s32 amount)
{
	if (m_isDead)
		return;

	m_health -= amount;

	if (m_health <= 0)
	{
		m_health = 0;
		m_isDead = true;

		if (m_playerNode)
		{
			m_playerNode->setMD2Animation(EMAT_DEATH_FALLBACK);
			m_playerNode->setLoopMode(false);
		}

		if (m_weaponNode)
			m_weaponNode->setVisible(false);
	}
	else
	{
		m_isInPain = true;
		m_isShooting = false;
		m_painTimer = PAIN_ANIM_DURATION;

		if (m_playerNode)
		{
			m_playerNode->setMD2Animation(EMAT_PAIN_A);
			if (m_weaponNode)
				m_weaponNode->setMD2Animation(EMAT_PAIN_A);
		}
	}
}

void Player::handleMovement(f32 deltaTime, InputHandler& input, f32 cameraYaw)
{
	bool movementInput =
		input.isKeyDown(KEY_KEY_W) ||
		input.isKeyDown(KEY_KEY_S) ||
		input.isKeyDown(KEY_KEY_A) ||
		input.isKeyDown(KEY_KEY_D);

	if (movementInput)
		m_rotationY = cameraYaw;

	if (!movementInput && input.consumeRightClick())
		m_rotationY = cameraYaw;

	f32 yawRad = m_rotationY * core::DEGTORAD;
	m_forward = vector3df(sinf(yawRad), 0, cosf(yawRad));
	m_right = vector3df(cosf(yawRad), 0, -sinf(yawRad));

	vector3df moveDir(0, 0, 0);
	bool moving = false;

	if (input.isKeyDown(KEY_KEY_W)) { moveDir += m_forward; moving = true; }
	if (input.isKeyDown(KEY_KEY_S)) { moveDir -= m_forward; moving = true; }
	if (input.isKeyDown(KEY_KEY_A)) { moveDir -= m_right;   moving = true; }
	if (input.isKeyDown(KEY_KEY_D)) { moveDir += m_right;   moving = true; }

	if (moveDir.getLength() > 0)
		moveDir.normalize();

	if (m_isShooting)
	{
		moveDir = vector3df(0, 0, 0);
		moving = false;
	}

	// Move by updating position and syncing to kinematic body
	vector3df newPos = getPosition() + moveDir * PLAYER_SPEED * deltaTime;
	setPosition(newPos);

	updateAnimation(moving);
}

void Player::handleShooting(f32 deltaTime, InputHandler& input)
{
	m_shootCooldown -= deltaTime;
	if (m_shootCooldown < 0) m_shootCooldown = 0;

	if (m_attackAnimTimer > 0)
	{
		m_attackAnimTimer -= deltaTime;
		if (m_attackAnimTimer <= 0)
		{
			m_isShooting = false;
			if (m_playerNode)
			{
				m_playerNode->setMD2Animation(m_isMoving ? EMAT_RUN : EMAT_STAND);
				if (m_weaponNode)
					m_weaponNode->setMD2Animation(m_isMoving ? EMAT_RUN : EMAT_STAND);
			}
		}
	}

	if (input.isLeftMouseDown() && m_shootCooldown <= 0)
	{
		if (m_ammo <= 0)
		{
			m_shootCooldown = WAVE_ANIM_DURATION;
			m_attackAnimTimer = WAVE_ANIM_DURATION;
			m_isShooting = true;

			if (m_playerNode)
			{
				m_playerNode->setMD2Animation(EMAT_WAVE);
				if (m_weaponNode) m_weaponNode->setMD2Animation(EMAT_WAVE);
			}
		}
		else
		{
			m_ammo--;
			m_shootCooldown = GUN_FIRE_RATE;
			m_attackAnimTimer = ATTACK_ANIM_DURATION;
			m_isShooting = true;

			if (m_playerNode)
			{
				m_playerNode->setMD2Animation(EMAT_ATTACK);
				if (m_weaponNode) m_weaponNode->setMD2Animation(EMAT_ATTACK);
			}

			// Bullet raycast from chest height along forward direction
			vector3df rayStart = getPosition() + vector3df(0, 30, 0);
			vector3df rayEnd = rayStart + m_forward * SHOOT_RANGE;

			m_debugRay = { rayStart, rayEnd, true, ATTACK_ANIM_DURATION };

			Physics::RayResult result = m_physics->rayTest(rayStart, rayEnd);

			if (result.hasHit && result.hitObject != m_body)
			{
				m_lastHitObject = result.hitObject;
			}
		}
	}
}

void Player::updateAnimation(bool moving)
{
	if (m_isShooting)
	{
		m_isMoving = moving;
		return;
	}

	if (moving && !m_isMoving)
	{
		if (m_playerNode) m_playerNode->setMD2Animation(EMAT_RUN);
		if (m_weaponNode) m_weaponNode->setMD2Animation(EMAT_RUN);
	}
	else if (!moving && m_isMoving)
	{
		if (m_playerNode) m_playerNode->setMD2Animation(EMAT_STAND);
		if (m_weaponNode) m_weaponNode->setMD2Animation(EMAT_STAND);
	}

	m_isMoving = moving;
}
