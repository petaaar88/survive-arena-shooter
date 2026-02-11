#include "GameObject.h"

GameObject::GameObject(ISceneNode* node, btRigidBody* body)
	: m_node(node)
	, m_body(body)
	, m_alive(true)
	, m_removeMe(false)
{
	if (m_body)
		m_body->setUserPointer(this);
}

GameObject::~GameObject()
{
}

void GameObject::syncPhysicsToNode()
{
	if (!m_body || !m_node)
		return;

	btTransform transform;
	m_body->getMotionState()->getWorldTransform(transform);

	btVector3 pos = transform.getOrigin();
	m_node->setPosition(toIrrlicht(pos));
}

void GameObject::syncNodeToPhysics()
{
	if (!m_body || !m_node)
		return;

	vector3df pos = m_node->getPosition();
	btTransform transform;
	transform.setIdentity();
	transform.setOrigin(toBullet(pos));

	m_body->setWorldTransform(transform);
	m_body->getMotionState()->setWorldTransform(transform);
}

vector3df GameObject::getPosition() const
{
	if (m_body)
	{
		btTransform transform;
		m_body->getMotionState()->getWorldTransform(transform);
		return toIrrlicht(transform.getOrigin());
	}
	if (m_node)
		return m_node->getPosition();
	return vector3df(0, 0, 0);
}

void GameObject::setPosition(const vector3df& pos)
{
	if (m_node)
		m_node->setPosition(pos);

	if (m_body)
	{
		btTransform transform;
		transform.setIdentity();
		transform.setOrigin(toBullet(pos));
		m_body->setWorldTransform(transform);
		m_body->getMotionState()->setWorldTransform(transform);
	}
}
