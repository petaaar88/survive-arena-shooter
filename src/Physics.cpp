#include "Physics.h"

Physics::Physics()
{
	m_collisionConfig = new btDefaultCollisionConfiguration();
	m_dispatcher = new btCollisionDispatcher(m_collisionConfig);
	m_broadphase = new btDbvtBroadphase();
	m_solver = new btSequentialImpulseConstraintSolver();
	m_world = new btDiscreteDynamicsWorld(m_dispatcher, m_broadphase, m_solver, m_collisionConfig);
	m_world->setGravity(btVector3(0, -981.0f, 0));

	// Required for btGhostObject overlap detection
	m_broadphase->getOverlappingPairCache()->setInternalGhostPairCallback(new btGhostPairCallback());
}

Physics::~Physics()
{
	// Remove all remaining rigid bodies
	for (int i = m_world->getNumCollisionObjects() - 1; i >= 0; i--)
	{
		btCollisionObject* obj = m_world->getCollisionObjectArray()[i];
		btRigidBody* body = btRigidBody::upcast(obj);
		if (body && body->getMotionState())
			delete body->getMotionState();
		m_world->removeCollisionObject(obj);
		delete obj;
	}

	delete m_world;
	delete m_solver;
	delete m_broadphase;
	delete m_dispatcher;
	delete m_collisionConfig;
}

void Physics::stepSimulation(f32 deltaTime, int maxSubSteps)
{
	m_world->stepSimulation(deltaTime, maxSubSteps);
}

btRigidBody* Physics::createRigidBody(f32 mass, btCollisionShape* shape, const vector3df& position)
{
	btVector3 localInertia(0, 0, 0);
	if (mass > 0.0f)
		shape->calculateLocalInertia(mass, localInertia);

	btTransform startTransform;
	startTransform.setIdentity();
	startTransform.setOrigin(toBullet(position));

	btDefaultMotionState* motionState = new btDefaultMotionState(startTransform);

	btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, motionState, shape, localInertia);
	btRigidBody* body = new btRigidBody(rbInfo);

	m_world->addRigidBody(body);
	return body;
}

void Physics::removeRigidBody(btRigidBody* body)
{
	if (!body) return;

	m_world->removeRigidBody(body);

	if (body->getMotionState())
		delete body->getMotionState();

	if (body->getCollisionShape())
		delete body->getCollisionShape();

	delete body;
}

void Physics::setDebugDrawer(btIDebugDraw* drawer)
{
	m_world->setDebugDrawer(drawer);
}

void Physics::debugDrawWorld()
{
	m_world->debugDrawWorld();
}

void Physics::addGhostObject(btGhostObject* ghost)
{
	if (ghost)
		m_world->addCollisionObject(ghost, btBroadphaseProxy::SensorTrigger,
			btBroadphaseProxy::AllFilter);
}

void Physics::removeGhostObject(btGhostObject* ghost)
{
	if (ghost)
		m_world->removeCollisionObject(ghost);
}

bool Physics::isGhostOverlapping(btGhostObject* ghost, btRigidBody* body)
{
	if (!ghost || !body)
		return false;

	for (int i = 0; i < ghost->getNumOverlappingObjects(); i++)
	{
		if (ghost->getOverlappingObject(i) == body)
			return true;
	}
	return false;
}

Physics::RayResult Physics::rayTest(const vector3df& from, const vector3df& to)
{
	RayResult result;
	result.hasHit = false;
	result.hitObject = nullptr;
	result.hitPoint = btVector3(0, 0, 0);
	result.hitNormal = btVector3(0, 0, 0);

	btVector3 btFrom = toBullet(from);
	btVector3 btTo = toBullet(to);

	// Custom callback that skips ghost/trigger objects (CF_NO_CONTACT_RESPONSE)
	struct IgnoreGhostsRayCallback : public btCollisionWorld::ClosestRayResultCallback
	{
		IgnoreGhostsRayCallback(const btVector3& from, const btVector3& to)
			: ClosestRayResultCallback(from, to) {}

		btScalar addSingleResult(btCollisionWorld::LocalRayResult& rayResult, bool normalInWorldSpace) override
		{
			if (rayResult.m_collisionObject->getCollisionFlags() & btCollisionObject::CF_NO_CONTACT_RESPONSE)
				return 1.0f;
			return ClosestRayResultCallback::addSingleResult(rayResult, normalInWorldSpace);
		}
	};

	IgnoreGhostsRayCallback callback(btFrom, btTo);
	m_world->rayTest(btFrom, btTo, callback);

	if (callback.hasHit())
	{
		result.hasHit = true;
		result.hitObject = const_cast<btCollisionObject*>(callback.m_collisionObject);
		result.hitPoint = callback.m_hitPointWorld;
		result.hitNormal = callback.m_hitNormalWorld;
	}

	return result;
}
