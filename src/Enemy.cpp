#include "Enemy.h"
#include <cmath>
#include <cstdlib>
#include <vector>

static const f32 ENEMY_SPEED = 160.0f;
static const f32 ENEMY_FAST_SPEED = 270.0f;
static const f32 ENEMY_ATTACK_RANGE = 45.0f;
static const f32 ENEMY_CHASE_RANGE = 500.0f;
static const f32 ENEMY_ATTACK_COOLDOWN = 1.0f;
static const s32 ENEMY_HEALTH = 50;
static const s32 ENEMY_FAST_HEALTH = 25;
static const s32 ENEMY_DAMAGE = 10;
static const s32 ENEMY_FAST_DAMAGE = 5;
static const f32 ENEMY_DEATH_DURATION = 0.5f;
static const f32 ENEMY_PAIN_DURATION = 0.4f;
static const f32 MD2_ROTATION_OFFSET = -90.0f;
static const f32 ATTACK_TRIGGER_RADIUS = 15.0f;
static const f32 ATTACK_TRIGGER_FORWARD_OFFSET = 25.0f;
static const f32 STUCK_TIME_THRESHOLD = 1.0f;
static const f32 STUCK_DISTANCE_THRESHOLD = 5.0f;
static const f32 STRAFE_DURATION = 0.6f;
static const f32 SALUTE_DURATION = 2.0f;
static const f32 SALUTE_COOLDOWN_MIN = 3.0f;
static const f32 SALUTE_COOLDOWN_MAX = 8.0f;

// (0.0 = silent, 1.0 = max)
static const f32 ENEMY_SFX_SALUTE_VOLUME = 0.3f;
static const f32 ENEMY_SFX_ATTACK_VOLUME = 0.5f;
static const f32 ENEMY_SFX_HURT_VOLUME   = 0.5f;
static const f32 ENEMY_SFX_DEATH_VOLUME  = 0.6f;
static const int  MAX_CONCURRENT_SALUTES = 3;

static std::vector<irrklang::ISound*> s_activeSalutes;

static void playSaluteSound(irrklang::ISoundEngine* engine)
{
	if (!engine) return;

	// Clean up finished sounds
	for (auto it = s_activeSalutes.begin(); it != s_activeSalutes.end(); )
	{
		if ((*it)->isFinished())
		{
			(*it)->drop();
			it = s_activeSalutes.erase(it);
		}
		else
			++it;
	}

	if ((int)s_activeSalutes.size() >= MAX_CONCURRENT_SALUTES)
		return;

	irrklang::ISound* s = engine->play2D("assets/audio/enemies/salute.mp3", false, true, true);
	if (s)
	{
		s->setVolume(ENEMY_SFX_SALUTE_VOLUME);
		s->setIsPaused(false);
		s_activeSalutes.push_back(s);
	}
}

Enemy::Enemy(ISceneManager* smgr, IVideoDriver* driver, Physics* physics, const vector3df& spawnPos, const vector3df& forward, irrklang::ISoundEngine* soundEngine, EnemyType type)
	: GameObject(nullptr, nullptr)
	, m_type(type)
	, m_smgr(smgr)
	, m_driver(driver)
	, m_physics(physics)
	, m_soundEngine(soundEngine)
	, m_animNode(nullptr)
	, m_rotationY(0.0f)
	, m_state(EnemyState::IDLE)
	, m_isDead(false)
	, m_isMoving(false)
	, m_isInPain(false)
	, m_health(type == EnemyType::FAST ? ENEMY_FAST_HEALTH : ENEMY_HEALTH)
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
	, m_isSaluting(false)
	, m_saluteTimer(0.0f)
	, m_saluteCooldown(SALUTE_COOLDOWN_MIN + static_cast<f32>(rand()) / RAND_MAX * (SALUTE_COOLDOWN_MAX - SALUTE_COOLDOWN_MIN))
	, m_saluteAllowed(false)
	, m_spawnForward(forward)
	, m_spawnWalkDistance(150.0f)
	, m_spawnDistanceTraveled(0.0f)
	, m_physicsCreated(false)
{
	bool isGateSpawn = (forward.getLength() > 0.01f);

	IAnimatedMesh* mesh = smgr->getMesh("assets/models/enemy/tris.md2");
	if (mesh)
	{
		m_animNode = smgr->addAnimatedMeshSceneNode(mesh);
		if (m_animNode)
		{
			const char* texPath = (m_type == EnemyType::FAST)
				? "assets/models/enemy/ctf_r.pcx"
				: "assets/models/enemy/ctf_b.pcx";
			m_animNode->setMaterialTexture(0, driver->getTexture(texPath));
			m_animNode->setMaterialFlag(EMF_LIGHTING, false);
			m_animNode->setMaterialFlag(EMF_FOG_ENABLE, true);
			if (m_type == EnemyType::FAST)
				m_animNode->setScale(vector3df(1.2f, 1.2f, 1.2f));
			m_animNode->setPosition(spawnPos);

			if (isGateSpawn)
				m_animNode->setMD2Animation(EMAT_RUN);
			else
				m_animNode->setMD2Animation(EMAT_STAND);
		}
	}

	m_node = m_animNode;

	if (isGateSpawn)
	{
		// Gate spawn
		m_state = EnemyState::SPAWNING;
		m_rotationY = atan2f(forward.X, forward.Z) * core::RADTODEG;
	}
	else
	{
		// Normal spawn: create physics 
		createPhysicsBody(spawnPos);
	}
}

void Enemy::createPhysicsBody(const vector3df& pos)
{
	f32 capsuleRadius = (m_type == EnemyType::FAST) ? 18.0f : 15.0f;
	f32 capsuleHeight = (m_type == EnemyType::FAST) ? 36.0f : 30.0f;
	btCapsuleShape* capsule = new btCapsuleShape(capsuleRadius, capsuleHeight);
	m_body = m_physics->createRigidBody(10.0f, capsule, pos);
	m_body->setAngularFactor(btVector3(0, 0, 0));
	m_body->setActivationState(DISABLE_DEACTIVATION);
	m_body->setUserPointer(this);

	m_attackShape = new btSphereShape(ATTACK_TRIGGER_RADIUS);
	m_attackTrigger = new btGhostObject();
	m_attackTrigger->setCollisionShape(m_attackShape);
	m_attackTrigger->setCollisionFlags(btCollisionObject::CF_NO_CONTACT_RESPONSE);
	btTransform triggerTransform;
	triggerTransform.setIdentity();
	triggerTransform.setOrigin(toBullet(pos));
	m_attackTrigger->setWorldTransform(triggerTransform);
	m_physics->addGhostObject(m_attackTrigger);

	m_physicsCreated = true;
}

Enemy::~Enemy()
{
	if (m_physicsCreated)
	{
		if (m_body)
			m_physics->removeRigidBody(m_body);
		if (m_attackTrigger)
		{
			m_physics->removeGhostObject(m_attackTrigger);
			delete m_attackTrigger;
		}
		delete m_attackShape;
	}

	if (m_animNode)
		m_animNode->remove();
}

void Enemy::update(f32 deltaTime)
{
	if (m_physicsCreated)
	{
		syncPhysicsToNode();
		updateAttackTrigger();
	}

	if (m_animNode)
		m_animNode->setRotation(vector3df(0, m_rotationY + MD2_ROTATION_OFFSET, 0));
}

void Enemy::updateAttackTrigger()
{
	if (!m_attackTrigger || !m_body)
		return;

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
			if (m_body)
				m_body->setLinearVelocity(btVector3(0, m_body->getLinearVelocity().getY(), 0));
			return;
		}
	}

	if (m_state == EnemyState::SPAWNING)
	{
		f32 step = getSpeed() * deltaTime;
		vector3df currentPos = m_animNode->getPosition();
		currentPos += m_spawnForward * step;
		m_animNode->setPosition(currentPos);
		m_spawnDistanceTraveled += step;

		if (m_spawnDistanceTraveled >= m_spawnWalkDistance)
		{
			createPhysicsBody(currentPos);
			m_state = EnemyState::SALUTING;
			m_saluteTimer = SALUTE_DURATION;
			if (m_animNode) m_animNode->setMD2Animation(EMAT_SALUTE);
			playSaluteSound(m_soundEngine);
		}
		return;
	}

	if (m_state == EnemyState::SALUTING)
	{
		if (m_body)
			m_body->setLinearVelocity(btVector3(0, m_body->getLinearVelocity().getY(), 0));
		m_saluteTimer -= deltaTime;
		if (m_saluteTimer <= 0)
		{
			m_state = EnemyState::CHASE;
			m_isMoving = false;
		}
		return;
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
		if (m_isSaluting)
		{
			if (m_body)
				m_body->setLinearVelocity(btVector3(0, m_body->getLinearVelocity().getY(), 0));
			m_saluteTimer -= deltaTime;
			if (m_saluteTimer <= 0)
			{
				m_isSaluting = false;
				m_isMoving = false;
			}
			break;
		}

		m_saluteCooldown -= deltaTime;
		if (m_saluteCooldown < 0) m_saluteCooldown = 0;

		if (m_saluteAllowed && m_saluteCooldown <= 0)
		{
			m_isSaluting = true;
			m_saluteTimer = SALUTE_DURATION;
			m_saluteCooldown = SALUTE_COOLDOWN_MIN + static_cast<f32>(rand()) / RAND_MAX * (SALUTE_COOLDOWN_MAX - SALUTE_COOLDOWN_MIN);
			m_isMoving = false;
			if (m_body)
				m_body->setLinearVelocity(btVector3(0, m_body->getLinearVelocity().getY(), 0));
			if (m_animNode) m_animNode->setMD2Animation(EMAT_SALUTE);
			playSaluteSound(m_soundEngine);
			break;
		}

		vector3df dir = playerPos - pos;
		dir.Y = 0;
		if (dir.getLength() > 0)
			dir.normalize();

		vector3df moveDir = dir;

		if (m_isStrafing)
		{
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
			f32 distMoved = pos.getDistanceFrom(m_lastCheckedPos);
			if (distMoved < STUCK_DISTANCE_THRESHOLD)
			{
				m_stuckTimer += deltaTime;
				if (m_stuckTimer >= STUCK_TIME_THRESHOLD)
				{
					m_isStrafing = true;
					m_strafeTimer = STRAFE_DURATION;
					m_strafeDirection = (fmodf(pos.X + pos.Z, 2.0f) > 1.0f) ? 1.0f : -1.0f;
				}
			}
			else
			{
				m_stuckTimer = 0.0f;
				m_lastCheckedPos = pos;
			}
		}

		if (m_body)
		{
			f32 speed = getSpeed();
			btVector3 velocity(moveDir.X * speed,
				m_body->getLinearVelocity().getY(),
				moveDir.Z * speed);
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
		if (m_body)
			m_body->setLinearVelocity(btVector3(0, m_body->getLinearVelocity().getY(), 0));

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

		if (m_physicsCreated)
		{
			if (m_body)
			{
				m_physics->removeRigidBody(m_body);
				m_body = nullptr;
			}
			if (m_attackTrigger)
			{
				m_physics->removeGhostObject(m_attackTrigger);
				delete m_attackTrigger;
				m_attackTrigger = nullptr;
			}
			delete m_attackShape;
			m_attackShape = nullptr;
		}

		if (m_soundEngine)
		{
			irrklang::ISound* s = m_soundEngine->play2D("assets/audio/enemies/death.mp3", false, true, true);
			if (s) { s->setVolume(ENEMY_SFX_DEATH_VOLUME); s->setIsPaused(false); s->drop(); }
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
			m_animNode->setMD2Animation(EMAT_PAIN_A);
		if (m_soundEngine)
		{
			irrklang::ISound* s = m_soundEngine->play2D("assets/audio/enemies/hurt.mp3", false, true, true);
			if (s) { s->setVolume(ENEMY_SFX_HURT_VOLUME); s->setIsPaused(false); s->drop(); }
		}
	}
}

bool Enemy::wantsToDealDamage() const
{
	return m_state == EnemyState::ATTACK && m_attackCooldown <= 0 && !m_isDead;
}

s32 Enemy::getAttackDamage() const
{
	return (m_type == EnemyType::FAST) ? ENEMY_FAST_DAMAGE : ENEMY_DAMAGE;
}

f32 Enemy::getSpeed() const
{
	return (m_type == EnemyType::FAST) ? ENEMY_FAST_SPEED : ENEMY_SPEED;
}

void Enemy::resetAttackCooldown()
{
	m_attackCooldown = ENEMY_ATTACK_COOLDOWN;
	if (m_soundEngine)
	{
		irrklang::ISound* s = m_soundEngine->play2D("assets/audio/enemies/attack.mp3", false, true, true);
		if (s) { s->setVolume(ENEMY_SFX_ATTACK_VOLUME); s->setIsPaused(false); s->drop(); }
	}
}
