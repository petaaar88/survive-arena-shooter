#include "Enemy.h"
#include <cmath>

static const f32 ENEMY_SPEED = 80.0f;
static const f32 ENEMY_ATTACK_RANGE = 45.0f;
static const f32 ENEMY_CHASE_RANGE = 500.0f;
static const f32 ENEMY_ATTACK_COOLDOWN = 1.0f;
static const s32 ENEMY_HEALTH = 50;
static const s32 ENEMY_DAMAGE = 10;
static const f32 ENEMY_DEATH_DURATION = 0.5f;
static const f32 ENEMY_PAIN_DURATION = 0.4f;
static const f32 MD2_ROTATION_OFFSET = -90.0f;
static const f32 ATTACK_TRIGGER_RADIUS = 15.0f;
static const f32 ATTACK_TRIGGER_FORWARD_OFFSET = 25.0f;
static const f32 STUCK_TIME_THRESHOLD = 1.0f;
static const f32 STUCK_DISTANCE_THRESHOLD = 5.0f;
static const f32 STRAFE_DURATION = 0.6f;

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
	, m_attackAllowed(false)
	, m_attackTrigger(nullptr)
	, m_attackShape(nullptr)
	, m_lastCheckedPos(spawnPos)
	, m_stuckTimer(0.0f)
	, m_isStrafing(false)
	, m_strafeTimer(0.0f)
	, m_strafeDirection(1.0f)
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

	// Create attack trigger ghost object (positioned in front of enemy)
	m_attackShape = new btSphereShape(ATTACK_TRIGGER_RADIUS);
	m_attackTrigger = new btGhostObject();
	m_attackTrigger->setCollisionShape(m_attackShape);
	m_attackTrigger->setCollisionFlags(btCollisionObject::CF_NO_CONTACT_RESPONSE);
	btTransform triggerTransform;
	triggerTransform.setIdentity();
	triggerTransform.setOrigin(toBullet(spawnPos));
	m_attackTrigger->setWorldTransform(triggerTransform);
	physics->addGhostObject(m_attackTrigger);
}

Enemy::~Enemy()
{
	if (m_attackTrigger)
	{
		m_physics->removeGhostObject(m_attackTrigger);
		delete m_attackTrigger;
	}
	delete m_attackShape;

	if (m_animNode)
		m_animNode->remove();
}

void Enemy::update(f32 deltaTime)
{
	// Sync Bullet transform → Irrlicht node
	syncPhysicsToNode();

	if (m_animNode)
		m_animNode->setRotation(vector3df(0, m_rotationY + MD2_ROTATION_OFFSET, 0));

	updateAttackTrigger();
}

void Enemy::updateAttackTrigger()
{
	if (!m_attackTrigger || !m_body)
		return;

	// Compute forward direction from enemy's facing angle
	f32 yawRad = m_rotationY * core::DEGTORAD;
	btVector3 forward(sinf(yawRad), 0, cosf(yawRad));

	btTransform bodyTransform;
	m_body->getMotionState()->getWorldTransform(bodyTransform);
	btVector3 triggerPos = bodyTransform.getOrigin() + forward * ATTACK_TRIGGER_FORWARD_OFFSET;

	btTransform triggerTransform;
	triggerTransform.setIdentity();
	triggerTransform.setOrigin(triggerPos);
	m_attackTrigger->setWorldTransform(triggerTransform);
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

		vector3df moveDir = dir;

		if (m_isStrafing)
		{
			// Move perpendicular to player direction
			moveDir = vector3df(-dir.Z, 0, dir.X) * m_strafeDirection;
			m_strafeTimer -= deltaTime;
			if (m_strafeTimer <= 0)
			{
				m_isStrafing = false;
				m_stuckTimer = 0.0f;
				m_lastCheckedPos = pos;
			}
		}
		else
		{
			// Stuck detection: check if enemy barely moved
			f32 distMoved = pos.getDistanceFrom(m_lastCheckedPos);
			if (distMoved < STUCK_DISTANCE_THRESHOLD)
			{
				m_stuckTimer += deltaTime;
				if (m_stuckTimer >= STUCK_TIME_THRESHOLD)
				{
					m_isStrafing = true;
					m_strafeTimer = STRAFE_DURATION;
					// Alternate direction: pick based on a simple heuristic
					m_strafeDirection = (fmodf(pos.X + pos.Z, 2.0f) > 1.0f) ? 1.0f : -1.0f;
				}
			}
			else
			{
				m_stuckTimer = 0.0f;
				m_lastCheckedPos = pos;
			}
		}

		// Set velocity on Bullet body
		if (m_body)
		{
			btVector3 velocity(moveDir.X * ENEMY_SPEED,
				m_body->getLinearVelocity().getY(),
				moveDir.Z * ENEMY_SPEED);
			m_body->setLinearVelocity(velocity);
		}

		m_rotationY = atan2f(moveDir.X, moveDir.Z) * core::RADTODEG;

		if (!m_isMoving)
		{
			m_isMoving = true;
			if (m_animNode) m_animNode->setMD2Animation(EMAT_RUN);
		}

		if (distToPlayer < ENEMY_ATTACK_RANGE)
		{
			if (m_attackAllowed)
			{
				m_state = EnemyState::ATTACK;
				m_isMoving = false;
				m_isStrafing = false;
				m_stuckTimer = 0.0f;
				if (m_animNode) m_animNode->setMD2Animation(EMAT_ATTACK);
			}
			else
			{
				m_state = EnemyState::WAIT_ATTACK;
				m_isMoving = false;
				m_isStrafing = false;
				m_stuckTimer = 0.0f;
				if (m_body)
					m_body->setLinearVelocity(btVector3(0, m_body->getLinearVelocity().getY(), 0));
				if (m_animNode) m_animNode->setMD2Animation(EMAT_STAND);
			}
		}
		break;
	}

	case EnemyState::WAIT_ATTACK:
		// Stop movement, stand idle while waiting for attack turn
		if (m_body)
			m_body->setLinearVelocity(btVector3(0, m_body->getLinearVelocity().getY(), 0));

		// Face the player while waiting
		{
			vector3df dirToPlayer = playerPos - pos;
			dirToPlayer.Y = 0;
			if (dirToPlayer.getLength() > 0)
				m_rotationY = atan2f(dirToPlayer.X, dirToPlayer.Z) * core::RADTODEG;
		}

		if (m_attackAllowed && distToPlayer < ENEMY_ATTACK_RANGE)
		{
			m_state = EnemyState::ATTACK;
			if (m_animNode) m_animNode->setMD2Animation(EMAT_ATTACK);
		}
		else if (distToPlayer > ENEMY_ATTACK_RANGE * 1.5f)
		{
			m_state = EnemyState::CHASE;
			m_isMoving = false;
		}
		break;

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
		m_isStrafing = false;
		m_stuckTimer = 0.0f;

		if (m_animNode)
			m_animNode->setMD2Animation(EMAT_PAIN_A);
	}
}

bool Enemy::wantsToDealDamage() const
{
	return m_state == EnemyState::ATTACK && m_attackCooldown <= 0 && !m_isDead;
}

s32 Enemy::getAttackDamage() const
{
	return ENEMY_DAMAGE;
}

void Enemy::resetAttackCooldown()
{
	m_attackCooldown = ENEMY_ATTACK_COOLDOWN;
}
