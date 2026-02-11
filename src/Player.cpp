#include "Player.h"
#include <cmath>

static const f32 PLAYER_SPEED = 200.0f;
static const f32 GUN_FIRE_RATE = 0.8f;
static const f32 ATTACK_ANIM_DURATION = 0.5f;
static const f32 SHOOT_RANGE = 1000.0f;
static const f32 MD2_ROTATION_OFFSET = -90.0f;
static const s32 PLAYER_AMMO_START = 5;
static const s32 PLAYER_HEALTH_START = 100;
static const f32 WAVE_ANIM_DURATION = 0.5f;
static const f32 PAIN_ANIM_DURATION = 0.4f;

Player::Player(ISceneManager* smgr, IVideoDriver* driver)
	: m_smgr(smgr)
	, m_playerNode(nullptr)
	, m_weaponNode(nullptr)
	, m_position(0, 0, 0)
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
			m_playerNode->setPosition(m_position);
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
}

Player::~Player()
{
	// Irrlicht scene manager owns the nodes and cleans them up on device->drop().
	// Do not manually remove — the Player destructor runs after device->drop()
	// on stack-allocated players, and the node is already freed by then.
}

void Player::update(f32 deltaTime, InputHandler& input, f32 cameraYaw)
{
	if (m_isDead)
		return;

	// Tick pain animation
	if (m_isInPain)
	{
		m_painTimer -= deltaTime;
		if (m_painTimer <= 0)
		{
			m_isInPain = false;
			// Return to idle
			if (m_playerNode)
			{
				m_playerNode->setMD2Animation(m_isMoving ? EMAT_RUN : EMAT_STAND);
				if (m_weaponNode)
					m_weaponNode->setMD2Animation(m_isMoving ? EMAT_RUN : EMAT_STAND);
			}
		}
		else
		{
			// Still in pain — no movement or shooting
			if (m_playerNode)
			{
				m_playerNode->setPosition(m_position);
				m_playerNode->setRotation(vector3df(0, m_rotationY + MD2_ROTATION_OFFSET, 0));
			}
			return;
		}
	}

	handleMovement(deltaTime, input, cameraYaw);
	handleShooting(deltaTime, input);

	// Update node transform
	if (m_playerNode)
	{
		m_playerNode->setPosition(m_position);
		m_playerNode->setRotation(vector3df(0, m_rotationY + MD2_ROTATION_OFFSET, 0));
	}
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

		// Hide weapon
		if (m_weaponNode)
			m_weaponNode->setVisible(false);
	}
	else
	{
		// Play pain animation
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
	// Snap player rotation to camera when moving or right-clicking
	bool movementInput =
		input.isKeyDown(KEY_KEY_W) ||
		input.isKeyDown(KEY_KEY_S) ||
		input.isKeyDown(KEY_KEY_A) ||
		input.isKeyDown(KEY_KEY_D);

	if (movementInput)
		m_rotationY = cameraYaw;

	if (!movementInput && input.consumeRightClick())
		m_rotationY = cameraYaw;

	// Calculate direction vectors from player facing
	f32 yawRad = m_rotationY * core::DEGTORAD;
	m_forward = vector3df(sinf(yawRad), 0, cosf(yawRad));
	m_right = vector3df(cosf(yawRad), 0, -sinf(yawRad));

	// Build movement direction
	vector3df moveDir(0, 0, 0);
	bool moving = false;

	if (input.isKeyDown(KEY_KEY_W)) { moveDir += m_forward; moving = true; }
	if (input.isKeyDown(KEY_KEY_S)) { moveDir -= m_forward; moving = true; }
	if (input.isKeyDown(KEY_KEY_A)) { moveDir -= m_right;   moving = true; }
	if (input.isKeyDown(KEY_KEY_D)) { moveDir += m_right;   moving = true; }

	if (moveDir.getLength() > 0)
		moveDir.normalize();

	// Stop movement while shooting
	if (m_isShooting)
	{
		moveDir = vector3df(0, 0, 0);
		moving = false;
	}

	m_position += moveDir * PLAYER_SPEED * deltaTime;

	// Update animation
	updateAnimation(moving);
}

void Player::handleShooting(f32 deltaTime, InputHandler& input)
{
	m_shootCooldown -= deltaTime;
	if (m_shootCooldown < 0) m_shootCooldown = 0;

	// Tick attack animation timer
	if (m_attackAnimTimer > 0)
	{
		m_attackAnimTimer -= deltaTime;
		if (m_attackAnimTimer <= 0)
		{
			m_isShooting = false;
			// Return to appropriate animation
			if (m_playerNode)
			{
				m_playerNode->setMD2Animation(m_isMoving ? EMAT_RUN : EMAT_STAND);
				if (m_weaponNode)
					m_weaponNode->setMD2Animation(m_isMoving ? EMAT_RUN : EMAT_STAND);
			}
		}
	}

	// Fire
	if (input.isLeftMouseDown() && m_shootCooldown <= 0)
	{
		if (m_ammo <= 0)
		{
			// No ammo — play WAVE animation
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
			// Shoot
			m_ammo--;
			m_shootCooldown = GUN_FIRE_RATE;
			m_attackAnimTimer = ATTACK_ANIM_DURATION;
			m_isShooting = true;

			if (m_playerNode)
			{
				m_playerNode->setMD2Animation(EMAT_ATTACK);
				if (m_weaponNode) m_weaponNode->setMD2Animation(EMAT_ATTACK);
			}

			// Raycast from chest height along forward direction
			vector3df rayStart = m_position + vector3df(0, 30, 0);
			vector3df rayEnd = rayStart + m_forward * SHOOT_RANGE;

			line3d<f32> ray(rayStart, rayEnd);
			ISceneCollisionManager* collMan = m_smgr->getSceneCollisionManager();

			vector3df hitPoint;
			triangle3df hitTriangle;
			ISceneNode* hitNode = collMan->getSceneNodeAndCollisionPointFromRay(
				ray, hitPoint, hitTriangle, 0, 0);

			if (hitNode && hitNode != m_playerNode)
			{
				// Future: deal damage to enemy
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
