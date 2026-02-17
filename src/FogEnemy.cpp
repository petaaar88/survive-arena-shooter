#include "FogEnemy.h"
#include <cmath>
#include <iostream>
#include <vector>

static const f32 MOVEMENT_SPEED = 160.0f;
static const f32 POSITION_THRASHOLD = 25.0f;

static const f32 FOG_ENEMY_SPEED = 160.0f;
static const s32 FOG_ENEMY_HEALTH = 60;
static const f32 FOG_ENEMY_DEATH_DURATION = 0.8f;
static const f32 FOG_ENEMY_PAIN_DURATION = 0.4f;
static const f32 MD2_ROTATION_OFFSET = -90.0f;
static const f32 FALLBACK_DURATION = 2.0f;
static const f32 THROW_DURATION = 1.5f;

// Grenade
static const f32 GRENADE_SPEED = 300.0f;
static const f32 GRENADE_GRAVITY = 200.0f;
static const f32 GRENADE_SPAWN_HEIGHT = 30.0f;
static const f32 GRENADE_SIZE = 3.0f;

// Fog (Irrlicht EFT_FOG_LINEAR)
static const f32 FOG_DURATION = 10.0f;
static const f32 FOG_FADING_DURATION = 5.0f;
static const f32 FOG_START_INITIAL = 250.0f;
static const f32 FOG_END_INITIAL = 300.0f;
static const f32 FOG_START_FINAL = 9999.0f;
static const f32 FOG_END_FINAL = 10000.0f;
static const SColor FOG_COLOR(255, 180, 180, 180);

static const f32 STUCK_TIME_THRESHOLD = 1.0f;
static const f32 STUCK_DISTANCE_THRESHOLD = 5.0f;
static const f32 STRAFE_DURATION = 0.2f;

static const std::vector<vector3df> recoveringPositions = {
	{ 590.0f, 0, -367.0f },
	{ -340.0f, 0, 987.0f },
	{ -200.5f, 0,30.0f },
	{ 400.0f, 0,-120.0f }
};


FogEnemy::FogEnemy(ISceneManager* smgr, IVideoDriver* driver, Physics* physics,
	const vector3df& spawnPos, const vector3df& forward,
	irrklang::ISoundEngine* soundEngine)
	: GameObject(nullptr, nullptr)
	, m_smgr(smgr)
	, m_driver(driver)
	, m_physics(physics)
	, m_soundEngine(soundEngine)
	, m_animNode(nullptr)
	, m_rotationY(0.0f)
	, m_state(FogEnemyState::SPAWNING)
	, m_isDead(false)
	, m_isInPain(false)
	, m_health(FOG_ENEMY_HEALTH)
	, m_stateTimer(0.0f)
	, m_deathTimer(0.0f)
	, m_painTimer(0.0f)
	, m_spawnForward(forward)
	, m_spawnWalkDistance(150.0f)
	, m_spawnDistanceTraveled(0.0f)
	, m_physicsCreated(false)
	, m_grenadeActive(false)
	, m_grenadeNode(nullptr)
	, m_fogActive(false)
	, m_fogTimer(0.0f)
	, m_fogStartDist(FOG_START_FINAL)
	, m_fogEndDist(FOG_END_FINAL)
	, m_movementSpeed(MOVEMENT_SPEED)
	, m_lastCheckedPos(spawnPos)
	, m_stuckTimer(0.0f)
	, m_isStrafing(false)
	, m_strafeTimer(0.0f)
	, m_strafeDirection(1.0f)
{
	IAnimatedMesh* mesh = smgr->getMesh("assets/models/fog_enemy/tris.md2");
	if (mesh)
	{
		m_animNode = smgr->addAnimatedMeshSceneNode(mesh);
		if (m_animNode)
		{
			m_animNode->setMaterialTexture(0, driver->getTexture("assets/models/fog_enemy/default.pcx"));
			m_animNode->setMaterialFlag(EMF_LIGHTING, false);
			m_animNode->setMaterialFlag(EMF_FOG_ENABLE, true);
			m_animNode->setPosition(spawnPos);
			m_animNode->setMD2Animation(EMAT_RUN);
		}
	}

	m_node = m_animNode;
	m_rotationY = atan2f(forward.X, forward.Z) * core::RADTODEG;
}

void FogEnemy::createPhysicsBody(const vector3df& pos)
{
	btCapsuleShape* capsule = new btCapsuleShape(15.0f, 30.0f);
	m_body = m_physics->createRigidBody(10.0f, capsule, pos);
	m_body->setAngularFactor(btVector3(0, 0, 0));
	m_body->setActivationState(DISABLE_DEACTIVATION);
	m_body->setUserPointer(this);
	m_physicsCreated = true;
}

FogEnemy::~FogEnemy()
{
	if (m_physicsCreated && m_body)
		m_physics->removeRigidBody(m_body);

	if (m_animNode)
		m_animNode->remove();

	if (m_grenadeNode)
		m_grenadeNode->remove();

	// Disable fog if this enemy had it active
	if (m_fogActive && m_driver)
		m_driver->setFog(FOG_COLOR, EFT_FOG_LINEAR, FOG_START_FINAL, FOG_END_FINAL, 0.0f, true, false);
}

void FogEnemy::update(f32 deltaTime)
{

	if (m_physicsCreated)
		syncPhysicsToNode();

	if (m_animNode)
		m_animNode->setRotation(vector3df(0, m_rotationY + MD2_ROTATION_OFFSET, 0));

	updateGrenade(deltaTime);
	updateFog(deltaTime);
}

void FogEnemy::updateAI(f32 deltaTime, const vector3df& playerPos)
{
	if (shouldRemove())
		return;

	if (m_isDead)
	{
		m_deathTimer -= deltaTime;

		if(m_deathTimer <= 0)
			m_node->setVisible(false);

		if (m_deathTimer <= 0 && !m_fogActive)
			markForRemoval();
		
		
		if (m_body)
			m_body->setLinearVelocity(btVector3(0, m_body->getLinearVelocity().getY(), 0));
		return;
	}

	// Pain interrupts
	if (m_isInPain)
	{
		m_painTimer -= deltaTime;
		if (m_painTimer <= 0)
		{
			m_isInPain = false;
			if (m_state == FogEnemyState::IDLE && m_animNode) 
				m_animNode->setMD2Animation(EMAT_STAND);
			else if(m_state == FogEnemyState::REPOSITION && m_animNode)
				m_animNode->setMD2Animation(EMAT_RUN);

			m_animNode->setLoopMode(true);
		}
		else
		{
			if (m_body)
				m_body->setLinearVelocity(btVector3(0, m_body->getLinearVelocity().getY(), 0));
			return;
		}
	}

	switch (m_state)
	{
	case FogEnemyState::SPAWNING:
	{
		f32 step = FOG_ENEMY_SPEED * deltaTime;
		vector3df currentPos = m_animNode->getPosition();
		currentPos += m_spawnForward * step;
		m_animNode->setPosition(currentPos);
		m_spawnDistanceTraveled += step;

		if (m_spawnDistanceTraveled >= m_spawnWalkDistance)
		{
			createPhysicsBody(currentPos);
			m_state = FogEnemyState::FALLBACK;
			m_stateTimer = FALLBACK_DURATION;
			if (m_animNode)
			{
				m_animNode->setMD2Animation(EMAT_FALLBACK);
				m_animNode->setLoopMode(false);
			}
		}
		break;
	}

	case FogEnemyState::FALLBACK:
	{
		if (m_body)
			m_body->setLinearVelocity(btVector3(0, m_body->getLinearVelocity().getY(), 0));

		m_stateTimer -= deltaTime;
		if (m_stateTimer <= 0)
		{
			m_state = FogEnemyState::THROWING;
			m_stateTimer = THROW_DURATION;
			if (m_animNode)
			{
				m_animNode->setMD2Animation(EMAT_ATTACK);
				m_animNode->setLoopMode(false);
			}
			spawnGrenade();
		}
		break;
	}

	case FogEnemyState::THROWING:
	{
		if (m_body)
			m_body->setLinearVelocity(btVector3(0, m_body->getLinearVelocity().getY(), 0));

		m_stateTimer -= deltaTime;
		if (m_stateTimer <= 0)
		{
			if (m_animNode)
			{
				m_animNode->setMD2Animation(EMAT_RUN);
				m_animNode->setLoopMode(true);
			}



			//TODO: make it random
			m_currentRepositionIndex = 1;
			m_state = FogEnemyState::REPOSITION;
			
		}
		break;
	}

	case FogEnemyState::IDLE:
	{
		
		if (m_body)
			m_body->setLinearVelocity(btVector3(0, m_body->getLinearVelocity().getY(), 0));
		break;
	}

	case FogEnemyState::REPOSITION:
	{

		vector3df pos = getPosition();
		f32 distToPosition = pos.getDistanceFrom(recoveringPositions[m_currentRepositionIndex]);

		vector3df dir = recoveringPositions[m_currentRepositionIndex] - pos;
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

		if (m_body)
		{
			f32 speed = MOVEMENT_SPEED;
			btVector3 velocity(moveDir.X * speed,
				m_body->getLinearVelocity().getY(),
				moveDir.Z * speed);
			m_body->setLinearVelocity(velocity);
		}

		m_rotationY = atan2f(moveDir.X, moveDir.Z) * core::RADTODEG;

		if (distToPosition < POSITION_THRASHOLD)
		{
			
			m_state = FogEnemyState::IDLE;
			m_isStrafing = false;
			m_stuckTimer = 0.0f;
			if (m_animNode) m_animNode->setMD2Animation(EMAT_STAND);
			if (m_body)
				m_body->setLinearVelocity(btVector3(0, m_body->getLinearVelocity().getY(), 0));

			m_state = FogEnemyState::IDLE;
		}

		break;
	}

	case FogEnemyState::DEAD:
		break;
	}
}

void FogEnemy::takeDamage(s32 amount)
{
	if (m_isDead)
		return;

	m_health -= amount;

	if (m_health <= 0)
	{
		m_health = 0;
		m_isDead = true;
		m_state = FogEnemyState::DEAD;
		m_deathTimer = FOG_ENEMY_DEATH_DURATION;

		if (m_animNode)
		{
			m_animNode->setMD2Animation(EMAT_DEATH_FALLBACK);
			m_animNode->setLoopMode(false);
		}

		if (m_physicsCreated && m_body)
		{
			m_physics->removeRigidBody(m_body);
			m_body = nullptr;
		}
	}
	else
	{
		m_isInPain = true;
		m_painTimer = FOG_ENEMY_PAIN_DURATION;

		if (m_animNode) {
			m_animNode->setMD2Animation(EMAT_PAIN_B);
			m_animNode->setLoopMode(false);

		}
	}
}

void FogEnemy::spawnGrenade()
{
	if (!m_smgr || m_grenadeActive)
		return;

	vector3df pos = getPosition();
	pos.Y += GRENADE_SPAWN_HEIGHT;

	m_grenadeNode = m_smgr->addSphereSceneNode(GRENADE_SIZE);
	if (m_grenadeNode)
	{
		m_grenadeNode->setPosition(pos);
		m_grenadeNode->setMaterialFlag(EMF_LIGHTING, false);
		m_grenadeNode->setMaterialTexture(0, m_driver->getTexture("assets/models/fog_enemy/default.pcx"));
	}

	f32 yawRad = m_rotationY * core::DEGTORAD;
	m_grenadeVelocity = vector3df(sinf(yawRad) * GRENADE_SPEED, 100.0f, cosf(yawRad) * GRENADE_SPEED);
	m_grenadeActive = true;
}

void FogEnemy::updateGrenade(f32 deltaTime)
{
	if (!m_grenadeActive || !m_grenadeNode)
		return;

	m_grenadeVelocity.Y -= GRENADE_GRAVITY * deltaTime;

	vector3df pos = m_grenadeNode->getPosition();
	pos += m_grenadeVelocity * deltaTime;
	m_grenadeNode->setPosition(pos);

	if (pos.Y <= 0.0f)
	{
		activateFog();

		m_grenadeNode->remove();
		m_grenadeNode = nullptr;
		m_grenadeActive = false;
	}
}

void FogEnemy::activateFog()
{
	m_fogActive = true;
	m_fogTimer = FOG_DURATION;
	m_fogStartDist = FOG_START_INITIAL;
	m_fogEndDist = FOG_END_INITIAL;

	if (m_driver)
		m_driver->setFog(FOG_COLOR, EFT_FOG_LINEAR, m_fogStartDist, m_fogEndDist, 0.0f, true, false);
}

void FogEnemy::updateFog(f32 deltaTime)
{
	if (!m_fogActive)
		return;
	
	m_fogTimer -= deltaTime;


	if (m_fogTimer <= 0.0f && !m_fogFinished) {
		m_fogFinished = true;
		m_fogTimer = FOG_DURATION;
	}
	else if(!m_fogFinished)
		return;

	if (m_fogTimer <= 0.0f)
	{
		// Fog fully cleared
		m_fogActive = false;
		m_fogFinished = false;
		m_fogStartDist = FOG_START_FINAL;
		m_fogEndDist = FOG_END_FINAL;
		m_fogTimer = FOG_DURATION;
	}
	else
	{
		// Lerp from initial to final based on elapsed time
		f32 t = 1.0f - (m_fogTimer / FOG_DURATION);
		m_fogStartDist = FOG_START_INITIAL + (FOG_START_FINAL - FOG_START_INITIAL) * t;
		m_fogEndDist = FOG_END_INITIAL + (FOG_END_FINAL - FOG_END_INITIAL) * t;
	}

	if (m_driver)
		m_driver->setFog(FOG_COLOR, EFT_FOG_LINEAR, m_fogStartDist, m_fogEndDist, 0.0f, true, false);
}
