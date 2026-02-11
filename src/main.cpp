#include <irrlicht.h>

using namespace irr;
using namespace core;
using namespace scene;
using namespace video;
using namespace gui;

// Game constants
const f32 PLAYER_SPEED = 200.0f;
const f32 MOUSE_SENSITIVITY = 0.2f;
const f32 CAMERA_DISTANCE = 100.0f;
const f32 CAMERA_HEIGHT = 80.0f;

// MD2 model rotation offset — MD2 models face +X by default in Irrlicht,
// so we add -90 degrees to align model forward with our forward vector.
const f32 MD2_ROTATION_OFFSET = -90.0f;

class InputHandler : public IEventReceiver
{
public:
	InputHandler()
	{
		for (u32 i = 0; i < KEY_KEY_CODES_COUNT; ++i)
			m_keys[i] = false;
	}

	virtual bool OnEvent(const SEvent& event) override
	{
		if (event.EventType == EET_KEY_INPUT_EVENT)
		{
			m_keys[event.KeyInput.Key] = event.KeyInput.PressedDown;
		}
		return false;
	}

	bool isKeyDown(EKEY_CODE key) const { return m_keys[key]; }

private:
	bool m_keys[KEY_KEY_CODES_COUNT];
};

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

	// -- Load player model --
	IAnimatedMesh* playerMesh = smgr->getMesh("assets/models/player/tris.md2");
	if (!playerMesh)
	{
		device->drop();
		return 1;
	}

	IAnimatedMeshSceneNode* playerNode = smgr->addAnimatedMeshSceneNode(playerMesh);
	if (playerNode)
	{
		playerNode->setMaterialTexture(0, driver->getTexture("assets/models/player/caleb.pcx"));
		playerNode->setMaterialFlag(EMF_LIGHTING, false);
		playerNode->setPosition(vector3df(0, 0, 0));
		playerNode->setMD2Animation(EMAT_STAND);
		playerNode->setScale(vector3df(1.0f, 1.0f, 1.0f));
	}

	// -- Load weapon model, attach to player --
	IAnimatedMesh* weaponMesh = smgr->getMesh("assets/models/player/weapon.md2");
	IAnimatedMeshSceneNode* weaponNode = nullptr;
	if (weaponMesh)
	{
		weaponNode = smgr->addAnimatedMeshSceneNode(weaponMesh, playerNode);
		if (weaponNode)
		{
			weaponNode->setMaterialTexture(0, driver->getTexture("assets/models/player/Weapon.pcx"));
			weaponNode->setMaterialFlag(EMF_LIGHTING, false);
			weaponNode->setMD2Animation(EMAT_STAND);
		}
	}

	// -- Add camera (third-person) --
	ICameraSceneNode* camera = smgr->addCameraSceneNode();
	camera->setPosition(vector3df(0, CAMERA_HEIGHT, -CAMERA_DISTANCE));
	camera->setTarget(vector3df(0, 30, 0));

	// -- Add a ground plane so movement is visible --
	ISceneNode* ground = smgr->addCubeSceneNode(10.0f);
	if (ground)
	{
		ground->setScale(vector3df(100.0f, 0.1f, 100.0f));
		ground->setPosition(vector3df(0, -5, 0));
		ground->setMaterialFlag(EMF_LIGHTING, false);
		ground->setMaterialTexture(0, driver->getTexture("assets/models/player/caleb.pcx")); // reuse as ground texture
	}

	// -- Add ambient light --
	smgr->addLightSceneNode(0, vector3df(0, 200, 0), SColorf(1.0f, 1.0f, 1.0f), 800.0f);
	smgr->setAmbientLight(SColorf(0.3f, 0.3f, 0.3f));

	// Player state
	vector3df playerPos(0, 0, 0);
	f32 playerRotY = 0.0f; // yaw angle in degrees

	// Timing
	u32 lastTime = device->getTimer()->getTime();

	bool isMoving = false;

	// Cache screen center for mouse polling
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

		// Clamp delta to avoid huge jumps
		if (deltaTime > 0.1f) deltaTime = 0.1f;

		// -- Mouse look (poll cursor offset from center, then re-center) --
		position2d<s32> cursorPos = device->getCursorControl()->getPosition();
		f32 mouseDX = (f32)(cursorPos.X - centerX);
		playerRotY += mouseDX * MOUSE_SENSITIVITY;
		device->getCursorControl()->setPosition(centerX, centerY);

		// -- WASD movement relative to player facing direction --
		f32 yawRad = playerRotY * core::DEGTORAD;
		vector3df forward(sinf(yawRad), 0, cosf(yawRad));
		vector3df right(cosf(yawRad), 0, -sinf(yawRad));

		vector3df moveDir(0, 0, 0);
		bool moving = false;

		if (input.isKeyDown(KEY_KEY_W))
		{
			moveDir += forward;
			moving = true;
		}
		if (input.isKeyDown(KEY_KEY_S))
		{
			moveDir -= forward;
			moving = true;
		}
		if (input.isKeyDown(KEY_KEY_A))
		{
			moveDir -= right;
			moving = true;
		}
		if (input.isKeyDown(KEY_KEY_D))
		{
			moveDir += right;
			moving = true;
		}

		// Normalize diagonal movement
		if (moveDir.getLength() > 0)
			moveDir.normalize();

		playerPos += moveDir * PLAYER_SPEED * deltaTime;

		// -- Update player node --
		if (playerNode)
		{
			playerNode->setPosition(playerPos);
			playerNode->setRotation(vector3df(0, playerRotY + MD2_ROTATION_OFFSET, 0));

			// Switch animation based on movement
			if (moving && !isMoving)
			{
				playerNode->setMD2Animation(EMAT_RUN);
				if (weaponNode)
					weaponNode->setMD2Animation(EMAT_RUN);
			}
			else if (!moving && isMoving)
			{
				playerNode->setMD2Animation(EMAT_STAND);
				if (weaponNode)
					weaponNode->setMD2Animation(EMAT_STAND);
			}
			isMoving = moving;
		}

		// -- Update camera (follow behind player) --
		vector3df camOffset = -forward * CAMERA_DISTANCE + vector3df(0, CAMERA_HEIGHT, 0);
		vector3df camPos = playerPos + camOffset;
		vector3df camTarget = playerPos + vector3df(0, 30, 0);

		camera->setPosition(camPos);
		camera->setTarget(camTarget);

		// -- Render --
		driver->beginScene(true, true, SColor(255, 100, 130, 180));
		smgr->drawAll();
		driver->endScene();

		// ESC to quit
		if (input.isKeyDown(KEY_ESCAPE))
			device->closeDevice();
	}

	device->drop();
	return 0;
}
