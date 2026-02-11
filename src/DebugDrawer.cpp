#include "DebugDrawer.h"

DebugDrawer::DebugDrawer(IVideoDriver* driver)
	: m_driver(driver)
	, m_debugMode(DBG_DrawWireframe)
{
}

void DebugDrawer::beginDraw()
{
	// Reset world transform to identity so lines draw in world space
	m_driver->setTransform(irr::video::ETS_WORLD, irr::core::matrix4());

	// Set material with no lighting so lines are visible
	irr::video::SMaterial material;
	material.Lighting = false;
	material.Thickness = 1.0f;
	m_driver->setMaterial(material);
}

void DebugDrawer::drawLine(const btVector3& from, const btVector3& to, const btVector3& color)
{
	// Override Bullet's color with yellow for all wireframes
	SColor irrColor(255, 255, 255, 0);

	m_driver->draw3DLine(
		core::vector3df(from.getX(), from.getY(), from.getZ()),
		core::vector3df(to.getX(), to.getY(), to.getZ()),
		irrColor);
}

void DebugDrawer::drawContactPoint(const btVector3& pointOnB, const btVector3& normalOnB,
	btScalar distance, int lifeTime, const btVector3& color)
{
	drawLine(pointOnB, pointOnB + normalOnB * distance, color);
}

void DebugDrawer::reportErrorWarning(const char* warningString)
{
}

void DebugDrawer::draw3dText(const btVector3& location, const char* textString)
{
}

void DebugDrawer::setDebugMode(int debugMode)
{
	m_debugMode = debugMode;
}

int DebugDrawer::getDebugMode() const
{
	return m_debugMode;
}
