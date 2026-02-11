#pragma once
#include <btBulletDynamicsCommon.h>
#include <irrlicht.h>

using namespace irr;
using namespace core;

inline btVector3 toBullet(const vector3df& v) { return btVector3(v.X, v.Y, v.Z); }
inline vector3df toIrrlicht(const btVector3& v) { return vector3df(v.getX(), v.getY(), v.getZ()); }

class Physics
{
public:
	Physics();
	~Physics();

	void stepSimulation(f32 deltaTime, int maxSubSteps = 10);

	btRigidBody* createRigidBody(f32 mass, btCollisionShape* shape, const vector3df& position);
	void removeRigidBody(btRigidBody* body);

	struct RayResult
	{
		bool hasHit;
		btCollisionObject* hitObject;
		btVector3 hitPoint;
		btVector3 hitNormal;
	};

	RayResult rayTest(const vector3df& from, const vector3df& to);

	void setDebugDrawer(btIDebugDraw* drawer);
	void debugDrawWorld();

	btDiscreteDynamicsWorld* getWorld() { return m_world; }

private:
	btDefaultCollisionConfiguration*     m_collisionConfig;
	btCollisionDispatcher*               m_dispatcher;
	btBroadphaseInterface*               m_broadphase;
	btSequentialImpulseConstraintSolver*  m_solver;
	btDiscreteDynamicsWorld*             m_world;
};
