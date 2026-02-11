#pragma once
#include <btBulletDynamicsCommon.h>
#include <irrlicht.h>

using namespace irr;
using namespace video;

class DebugDrawer : public btIDebugDraw
{
public:
	DebugDrawer(IVideoDriver* driver);

	void beginDraw();
	void drawLine(const btVector3& from, const btVector3& to, const btVector3& color) override;
	void drawContactPoint(const btVector3& pointOnB, const btVector3& normalOnB,
		btScalar distance, int lifeTime, const btVector3& color) override;
	void reportErrorWarning(const char* warningString) override;
	void draw3dText(const btVector3& location, const char* textString) override;
	void setDebugMode(int debugMode) override;
	int getDebugMode() const override;

private:
	IVideoDriver* m_driver;
	int m_debugMode;
};
