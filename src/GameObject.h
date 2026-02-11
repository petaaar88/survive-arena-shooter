#pragma once
#include <irrlicht.h>
#include <btBulletDynamicsCommon.h>
#include "Physics.h"

using namespace irr;
using namespace core;
using namespace scene;

class GameObject
{
public:
	GameObject(ISceneNode* node, btRigidBody* body);
	virtual ~GameObject();

	virtual void update(f32 deltaTime) = 0;

	void syncPhysicsToNode();
	void syncNodeToPhysics();

	vector3df getPosition() const;
	void setPosition(const vector3df& pos);

	ISceneNode*  getNode() const { return m_node; }
	btRigidBody* getBody() const { return m_body; }

	bool isAlive() const { return m_alive; }
	void kill() { m_alive = false; }

	bool shouldRemove() const { return m_removeMe; }
	void markForRemoval() { m_removeMe = true; }

protected:
	ISceneNode*  m_node;
	btRigidBody* m_body;

	bool m_alive;
	bool m_removeMe;
};
