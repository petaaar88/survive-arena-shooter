#include "Pickup.h"

static const f32 PICKUP_RESPAWN_TIME = 30.0f;
static const f32 PICKUP_TRIGGER_RADIUS = 20.0f;
static const f32 PICKUP_VISUAL_SIZE = 10.0f;
static const f32 PICKUP_HOVER_HEIGHT = 10.0f;
static const f32 PICKUP_ROTATE_SPEED = 90.0f;

Pickup::Pickup(ISceneManager* smgr, IVideoDriver* driver, Physics* physics,
	const vector3df& position, PickupType type)
	: GameObject(nullptr, nullptr)
	, m_physics(physics)
	, m_type(type)
	, m_trigger(nullptr)
	, m_triggerShape(nullptr)
	, m_collected(false)
	, m_respawnTimer(0.0f)
	, m_spawnPos(position)
{
	// Visual: small cube floating above ground
	ISceneNode* cube = smgr->addCubeSceneNode(PICKUP_VISUAL_SIZE);
	if (cube)
	{
		cube->setPosition(vector3df(position.X, position.Y + PICKUP_HOVER_HEIGHT, position.Z));
		cube->setMaterialFlag(EMF_LIGHTING, false);

		// Yellow/orange color for ammo
		cube->setMaterialTexture(0, driver->getTexture("assets/textures/hud/bullet_icon.png"));
	}
	m_node = cube;

	// Ghost trigger for overlap detection
	m_triggerShape = new btSphereShape(PICKUP_TRIGGER_RADIUS);
	m_trigger = new btGhostObject();
	m_trigger->setCollisionShape(m_triggerShape);
	m_trigger->setCollisionFlags(btCollisionObject::CF_NO_CONTACT_RESPONSE);

	btTransform transform;
	transform.setIdentity();
	transform.setOrigin(toBullet(position));
	m_trigger->setWorldTransform(transform);

	physics->addGhostObject(m_trigger);
}

Pickup::~Pickup()
{
	if (m_trigger)
	{
		m_physics->removeGhostObject(m_trigger);
		delete m_trigger;
	}
	delete m_triggerShape;

	if (m_node)
		m_node->remove();
}

void Pickup::update(f32 deltaTime)
{
	if (m_collected)
	{
		m_respawnTimer -= deltaTime;
		if (m_respawnTimer <= 0)
		{
			m_collected = false;
			if (m_node)
				m_node->setVisible(true);
		}
		return;
	}

	// Rotate the pickup visually
	if (m_node)
	{
		vector3df rot = m_node->getRotation();
		rot.Y += PICKUP_ROTATE_SPEED * deltaTime;
		m_node->setRotation(rot);
	}
}

void Pickup::collect()
{
	m_collected = true;
	m_respawnTimer = PICKUP_RESPAWN_TIME;
	if (m_node)
		m_node->setVisible(false);
}
