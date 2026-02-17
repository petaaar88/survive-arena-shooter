#include "Powerup.h"

static const f32 POWERUP_VISUAL_SIZE = 15.0f;
static const f32 POWERUP_HOVER_HEIGHT = 15.0f;
static const f32 POWERUP_ROTATE_SPEED = 120.0f;
static const f32 POWERUP_TRIGGER_RADIUS = 25.0f;

static const char* getTextureForType(PowerupType type)
{
	switch (type)
	{
	case PowerupType::SPEED_BOOST:  return "assets/textures/powerups/speed_boost.png";
	case PowerupType::DAMAGE_BOOST: return "assets/textures/powerups/damage_boost.png";
	case PowerupType::GOD_MODE:     return "assets/textures/powerups/GOD_mode.png";
	}
	return "";
}

Powerup::Powerup(ISceneManager* smgr, IVideoDriver* driver, Physics* physics,
	const vector3df& position, PowerupType type)
	: GameObject(nullptr, nullptr)
	, m_physics(physics)
	, m_type(type)
	, m_trigger(nullptr)
	, m_triggerShape(nullptr)
	, m_collected(false)
	, m_spawnPos(position)
{
	// Visual: flat plane with texture on both sides
	// Create a plane mesh using Irrlicht's built-in mesh tools
	IMesh* planeMesh = smgr->getGeometryCreator()->createPlaneMesh(
		dimension2d<f32>(POWERUP_VISUAL_SIZE, POWERUP_VISUAL_SIZE), dimension2d<u32>(1, 1));

	if (planeMesh)
	{
		IMeshSceneNode* planeNode = smgr->addMeshSceneNode(planeMesh);
		planeMesh->drop();

		if (planeNode)
		{
			planeNode->setPosition(vector3df(position.X, position.Y + POWERUP_HOVER_HEIGHT, position.Z));
			planeNode->setMaterialFlag(EMF_LIGHTING, false);
			planeNode->setMaterialFlag(EMF_BACK_FACE_CULLING, false);
			planeNode->setMaterialFlag(EMF_FOG_ENABLE, true);
			planeNode->setMaterialTexture(0, driver->getTexture(getTextureForType(type)));
			planeNode->setMaterialType(EMT_TRANSPARENT_ALPHA_CHANNEL);

			// Rotate plane to be vertical (default is horizontal/flat on ground)
			planeNode->setRotation(vector3df(90, 0, 0));
		}
		m_node = planeNode;
	}

	// Ghost trigger for overlap detection
	m_triggerShape = new btSphereShape(POWERUP_TRIGGER_RADIUS);
	m_trigger = new btGhostObject();
	m_trigger->setCollisionShape(m_triggerShape);
	m_trigger->setCollisionFlags(btCollisionObject::CF_NO_CONTACT_RESPONSE);

	btTransform transform;
	transform.setIdentity();
	transform.setOrigin(toBullet(position));
	m_trigger->setWorldTransform(transform);

	physics->addGhostObject(m_trigger);
}

Powerup::~Powerup()
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

void Powerup::update(f32 deltaTime)
{
	if (m_collected)
		return;

	// Rotate on Y axis
	if (m_node)
	{
		vector3df rot = m_node->getRotation();
		rot.Y += POWERUP_ROTATE_SPEED * deltaTime;
		m_node->setRotation(rot);
	}
}

void Powerup::collect()
{
	m_collected = true;
	if (m_node)
		m_node->setVisible(false);
}

f32 Powerup::getDuration() const
{
	switch (m_type)
	{
	case PowerupType::SPEED_BOOST:  return SPEED_BOOST_DURATION;
	case PowerupType::DAMAGE_BOOST: return DAMAGE_BOOST_DURATION;
	case PowerupType::GOD_MODE:     return GOD_MODE_DURATION;
	}
	return 0.0f;
}
