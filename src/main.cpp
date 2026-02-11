#include <irrlicht.h>
#include "InputHandler.h"
#include "Player.h"

using namespace irr;
using namespace core;
using namespace scene;
using namespace video;

const f32 MOUSE_SENSITIVITY = 0.2f;
const f32 CAMERA_DISTANCE = 100.0f;
const f32 CAMERA_HEIGHT = 80.0f;

int main()
{
	InputHandler input;

	IrrlichtDevice* device =
		createDevice(video::EDT_OPENGL, dimension2d<u32>(800, 600), 16,
			false, false, false, &input);

	if (!device)
		return 1;

	device->setWindowCaption(L"Survive - Arena Shooter");
	device->getCursorControl()->setVisible(false);

	IVideoDriver* driver = device->getVideoDriver();
	ISceneManager* smgr = device->getSceneManager();

	// -- Create player --
	Player player(smgr, driver);

	// -- Camera --
	ICameraSceneNode* camera = smgr->addCameraSceneNode();

	// -- Ground plane --
	ISceneNode* ground = smgr->addCubeSceneNode(10.0f);
	if (ground)
	{
		ground->setScale(vector3df(100.0f, 0.1f, 100.0f));
		ground->setPosition(vector3df(0, -5, 0));
		ground->setMaterialFlag(EMF_LIGHTING, false);
		ground->setMaterialTexture(0, driver->getTexture("assets/models/player/caleb.pcx"));
	}

	// -- Lighting --
	smgr->addLightSceneNode(0, vector3df(0, 200, 0), SColorf(1.0f, 1.0f, 1.0f), 800.0f);
	smgr->setAmbientLight(SColorf(0.3f, 0.3f, 0.3f));

	// -- State --
	f32 cameraYaw = 0.0f;
	u32 lastTime = device->getTimer()->getTime();

	dimension2d<u32> screenSize = driver->getScreenSize();
	s32 centerX = screenSize.Width / 2;
	s32 centerY = screenSize.Height / 2;
	device->getCursorControl()->setPosition(centerX, centerY);

	while (device->run())
	{
		if (!device->isWindowActive())
		{
			device->yield();
			continue;
		}

		// Delta time
		u32 currentTime = device->getTimer()->getTime();
		f32 deltaTime = (currentTime - lastTime) / 1000.0f;
		lastTime = currentTime;
		if (deltaTime > 0.1f) deltaTime = 0.1f;

		// Mouse look
		position2d<s32> cursorPos = device->getCursorControl()->getPosition();
		f32 mouseDX = (f32)(cursorPos.X - centerX);
		cameraYaw += mouseDX * MOUSE_SENSITIVITY;
		device->getCursorControl()->setPosition(centerX, centerY);

		// Update player
		player.update(deltaTime, input, cameraYaw);

		// Update camera (orbit behind player)
		vector3df playerPos = player.getPosition();
		f32 camYawRad = cameraYaw * core::DEGTORAD;
		vector3df camForward(sinf(camYawRad), 0, cosf(camYawRad));
		vector3df camOffset = -camForward * CAMERA_DISTANCE + vector3df(0, CAMERA_HEIGHT, 0);

		camera->setPosition(playerPos + camOffset);
		camera->setTarget(playerPos + vector3df(0, 30, 0));

		// Render
		driver->beginScene(true, true, SColor(255, 100, 130, 180));
		smgr->drawAll();
		driver->endScene();

		if (input.isKeyDown(KEY_ESCAPE))
			device->closeDevice();
	}

	device->drop();
	return 0;
}
