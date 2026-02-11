#include "Enemy.h"
#include <cmath>

static const f32 ENEMY_SPEED = 80.0f;
static const f32 ENEMY_ATTACK_RANGE = 30.0f;
static const f32 ENEMY_CHASE_RANGE = 500.0f;
static const f32 ENEMY_ATTACK_COOLDOWN = 1.0f;
static const s32 ENEMY_HEALTH = 50;
static const s32 ENEMY_DAMAGE = 10;
static const f32 ENEMY_DEATH_DURATION = 1.5f;
static const f32 MD2_ROTATION_OFFSET = -90.0f;

Enemy::Enemy(ISceneManager* smgr, IVideoDriver* driver, const vector3df& spawnPos)
	: m_smgr(smgr)
	, m_node(nullptr)
	, m_position(spawnPos)
	, m_rotationY(0.0f)
	, m_state(EnemyState::IDLE)
	, m_isDead(false)
	, m_removeMe(false)
	, m_isMoving(false)
	, m_health(ENEMY_HEALTH)
	, m_attackCooldown(0.0f)
	, m_deathTimer(0.0f)
{
	IAnimatedMesh* mesh = smgr->getMesh("assets/models/enemy/tris.md2");
	if (mesh)
	{
		m_node = smgr->addAnimatedMeshSceneNode(mesh);
		if (m_node)
		{
			m_node->setMaterialTexture(0, driver->getTexture("assets/models/enemy/ctf_r.pcx"));
			m_node->setMaterialFlag(EMF_LIGHTING, false);
			m_node->setPosition(m_position);
			m_node->setMD2Animation(EMAT_STAND);
		}
	}
}

Enemy::~Enemy()
{
	if (m_node)
		m_node->remove();
}

void Enemy::update(f32 deltaTime, const vector3df& playerPos)
{
	if (m_removeMe)
		return;

	if (m_isDead)
	{
		m_deathTimer -= deltaTime;
		if (m_deathTimer <= 0)
			m_removeMe = true;
		return;
	}

	f32 distToPlayer = m_position.getDistanceFrom(playerPos);

	switch (m_state)
	{
	case EnemyState::IDLE:
		if (distToPlayer < ENEMY_CHASE_RANGE)
			m_state = EnemyState::CHASE;
		break;

	case EnemyState::CHASE:
	{
		// Face and move toward player
		vector3df dir = playerPos - m_position;
		dir.Y = 0;
		if (dir.getLength() > 0)
			dir.normalize();

		m_position += dir * ENEMY_SPEED * deltaTime;

		// Face the player
		m_rotationY = atan2f(dir.X, dir.Z) * core::RADTODEG;

		if (!m_isMoving)
		{
			m_isMoving = true;
			if (m_node) m_node->setMD2Animation(EMAT_RUN);
		}

		if (distToPlayer < ENEMY_ATTACK_RANGE)
		{
			m_state = EnemyState::ATTACK;
			m_isMoving = false;
			if (m_node) m_node->setMD2Animation(EMAT_ATTACK);
		}
		break;
	}

	case EnemyState::ATTACK:
		m_attackCooldown -= deltaTime;

		if (distToPlayer > ENEMY_ATTACK_RANGE * 1.5f)
		{
			m_state = EnemyState::CHASE;
			m_attackCooldown = 0;
		}
		break;

	case EnemyState::DEAD:
		break;
	}

	// Update node transform
	if (m_node)
	{
		m_node->setPosition(m_position);
		m_node->setRotation(vector3df(0, m_rotationY + MD2_ROTATION_OFFSET, 0));
	}
}

void Enemy::takeDamage(s32 amount)
{
	if (m_isDead)
		return;

	m_health -= amount;

	if (m_health <= 0)
	{
		m_health = 0;
		m_isDead = true;
		m_state = EnemyState::DEAD;
		m_deathTimer = ENEMY_DEATH_DURATION;

		if (m_node)
		{
			m_node->setMD2Animation(EMAT_DEATH_FALLBACK);
			m_node->setLoopMode(false);
		}
	}
	else
	{
		if (m_node)
			m_node->setMD2Animation(EMAT_PAIN_A);
	}
}
