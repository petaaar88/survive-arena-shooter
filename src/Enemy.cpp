#include "Enemy.h"
#include <cmath>

static const f32 ENEMY_SPEED = 80.0f;
static const f32 ENEMY_ATTACK_RANGE = 30.0f;
static const f32 ENEMY_CHASE_RANGE = 500.0f;
static const f32 ENEMY_ATTACK_COOLDOWN = 1.0f;
static const s32 ENEMY_HEALTH = 50;
static const s32 ENEMY_DAMAGE = 10;
static const f32 ENEMY_DEATH_DURATION = 1.5f;
static const f32 ENEMY_PAIN_DURATION = 0.4f;
static const f32 MD2_ROTATION_OFFSET = -90.0f;

Enemy::Enemy(ISceneManager* smgr, IVideoDriver* driver, Physics* physics, const vector3df& spawnPos)
	: GameObject(nullptr, nullptr)
	, m_smgr(smgr)
	, m_physics(physics)
	, m_animNode(nullptr)
	, m_rotationY(0.0f)
	, m_state(EnemyState::IDLE)
	, m_isDead(false)
	, m_isMoving(false)
	, m_isInPain(false)
	, m_health(ENEMY_HEALTH)
	, m_attackCooldown(0.0f)
	, m_deathTimer(0.0f)
	, m_painTimer(0.0f)
{
	IAnimatedMesh* mesh = smgr->getMesh("assets/models/enemy/tris.md2");
	if (mesh)
	{
		m_animNode = smgr->addAnimatedMeshSceneNode(mesh);
		if (m_animNode)
		{
			m_animNode->setMaterialTexture(0, driver->getTexture("assets/models/enemy/ctf_r.pcx"));
			m_animNode->setMaterialFlag(EMF_LIGHTING, false);
			m_animNode->setPosition(spawnPos);
			m_animNode->setMD2Animation(EMAT_STAND);
		}
	}

	// Create dynamic Bullet capsule
	btCapsuleShape* capsule = new btCapsuleShape(15.0f, 30.0f);
	m_body = physics->createRigidBody(10.0f, capsule, spawnPos);
	m_body->setAngularFactor(btVector3(0, 0, 0));
	m_body->setActivationState(DISABLE_DEACTIVATION);
	m_body->setUserPointer(this);

	m_node = m_animNode;
}

Enemy::~Enemy()
{
	if (m_animNode)
		m_animNode->remove();
}

void Enemy::update(f32 deltaTime)
{
	// Sync Bullet transform → Irrlicht node
	syncPhysicsToNode();

	if (m_animNode)
		m_animNode->setRotation(vector3df(0, m_rotationY + MD2_ROTATION_OFFSET, 0));
}

void Enemy::updateAI(f32 deltaTime, const vector3df& playerPos)
{
	if (shouldRemove())
		return;

	if (m_isDead)
	{
		m_deathTimer -= deltaTime;
		if (m_deathTimer <= 0)
			markForRemoval();

		// Stop movement on death
		if (m_body)
			m_body->setLinearVelocity(btVector3(0, m_body->getLinearVelocity().getY(), 0));
		return;
	}

	// Tick pain animation
	if (m_isInPain)
	{
		m_painTimer -= deltaTime;
		if (m_painTimer <= 0)
		{
			m_isInPain = false;
			m_state = EnemyState::CHASE;
			m_isMoving = false;
		}
		else
		{
			// Stop movement during pain
			if (m_body)
				m_body->setLinearVelocity(btVector3(0, m_body->getLinearVelocity().getY(), 0));
			return;
		}
	}

	vector3df pos = getPosition();
	f32 distToPlayer = pos.getDistanceFrom(playerPos);

	switch (m_state)
	{
	case EnemyState::IDLE:
		if (distToPlayer < ENEMY_CHASE_RANGE)
			m_state = EnemyState::CHASE;

		if (m_body)
			m_body->setLinearVelocity(btVector3(0, m_body->getLinearVelocity().getY(), 0));
		break;

	case EnemyState::CHASE:
	{
		vector3df dir = playerPos - pos;
		dir.Y = 0;
		if (dir.getLength() > 0)
			dir.normalize();

		// Set velocity on Bullet body
		if (m_body)
		{
			btVector3 velocity(dir.X * ENEMY_SPEED,
				m_body->getLinearVelocity().getY(),
				dir.Z * ENEMY_SPEED);
			m_body->setLinearVelocity(velocity);
		}

		m_rotationY = atan2f(dir.X, dir.Z) * core::RADTODEG;

		if (!m_isMoving)
		{
			m_isMoving = true;
			if (m_animNode) m_animNode->setMD2Animation(EMAT_RUN);
		}

		if (distToPlayer < ENEMY_ATTACK_RANGE)
		{
			m_state = EnemyState::ATTACK;
			m_isMoving = false;
			if (m_animNode) m_animNode->setMD2Animation(EMAT_ATTACK);
		}
		break;
	}

	case EnemyState::ATTACK:
		m_attackCooldown -= deltaTime;

		// Stop horizontal movement during attack
		if (m_body)
			m_body->setLinearVelocity(btVector3(0, m_body->getLinearVelocity().getY(), 0));

		if (distToPlayer > ENEMY_ATTACK_RANGE * 1.5f)
		{
			m_state = EnemyState::CHASE;
			m_attackCooldown = 0;
		}
		break;

	case EnemyState::DEAD:
		break;
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

		if (m_animNode)
		{
			m_animNode->setMD2Animation(EMAT_DEATH_FALLBACK);
			m_animNode->setLoopMode(false);
		}
	}
	else
	{
		m_isInPain = true;
		m_painTimer = ENEMY_PAIN_DURATION;
		m_isMoving = false;

		if (m_animNode)
			m_animNode->setMD2Animation(EMAT_PAIN_A);
	}
}
