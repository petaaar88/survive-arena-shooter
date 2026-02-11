#include <irrlicht.h>

using namespace irr;
using namespace core;
using namespace scene;
using namespace video;
using namespace gui;

const f32 PLAYER_SPEED = 200.0f;
const f32 MOUSE_SENSITIVITY = 0.2f;
const f32 CAMERA_DISTANCE = 100.0f;
const f32 CAMERA_HEIGHT = 80.0f;
const f32 GUN_FIRE_RATE = 0.8f;
const f32 ATTACK_ANIM_DURATION = 0.5f;
const f32 SHOOT_RANGE = 1000.0f;

const f32 MD2_ROTATION_OFFSET = -90.0f;

class InputHandler : public IEventReceiver
{
public:
	InputHandler() : m_leftMouseDown(false), m_rightMousePressed(false)
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
		else if (event.EventType == EET_MOUSE_INPUT_EVENT)
		{
			if (event.MouseInput.Event == EMIE_LMOUSE_PRESSED_DOWN)
				m_leftMouseDown = true;
			else if (event.MouseInput.Event == EMIE_LMOUSE_LEFT_UP)
				m_leftMouseDown = false;
			else if (event.MouseInput.Event == EMIE_RMOUSE_PRESSED_DOWN)
				m_rightMousePressed = true;
		}
		return false;
	}

	bool isKeyDown(EKEY_CODE key) const { return m_keys[key]; }
	bool isLeftMouseDown() const { return m_leftMouseDown; }

	bool consumeRightClick()
	{
		if (m_rightMousePressed)
		{
			m_rightMousePressed = false;
			return true;
		}
		return false;
	}

private:
	bool m_keys[KEY_KEY_CODES_COUNT];
	bool m_leftMouseDown;
	bool m_rightMousePressed;
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

	ICameraSceneNode* camera = smgr->addCameraSceneNode();
	camera->setPosition(vector3df(0, CAMERA_HEIGHT, -CAMERA_DISTANCE));
	camera->setTarget(vector3df(0, 30, 0));

	ISceneNode* ground = smgr->addCubeSceneNode(10.0f);
	if (ground)
	{
		ground->setScale(vector3df(100.0f, 0.1f, 100.0f));
		ground->setPosition(vector3df(0, -5, 0));
		ground->setMaterialFlag(EMF_LIGHTING, false);
		ground->setMaterialTexture(0, driver->getTexture("assets/models/player/caleb.pcx"));
	}

	smgr->addLightSceneNode(0, vector3df(0, 200, 0), SColorf(1.0f, 1.0f, 1.0f), 800.0f);
	smgr->setAmbientLight(SColorf(0.3f, 0.3f, 0.3f));

	vector3df playerPos(0, 0, 0);
	f32 playerRotY = 0.0f;
	f32 cameraYaw = 0.0f;

	u32 lastTime = device->getTimer()->getTime();

	bool isMoving = false;
	bool isShooting = false;
	f32 shootCooldown = 0.0f;
	f32 attackAnimTimer = 0.0f;

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

		u32 currentTime = device->getTimer()->getTime();
		f32 deltaTime = (currentTime - lastTime) / 1000.0f;
		lastTime = currentTime;

		if (deltaTime > 0.1f) deltaTime = 0.1f;

		position2d<s32> cursorPos = device->getCursorControl()->getPosition();
		f32 mouseDX = (f32)(cursorPos.X - centerX);
		cameraYaw += mouseDX * MOUSE_SENSITIVITY;
		device->getCursorControl()->setPosition(centerX, centerY);

		bool movementInput =
			input.isKeyDown(KEY_KEY_W) ||
			input.isKeyDown(KEY_KEY_S) ||
			input.isKeyDown(KEY_KEY_A) ||
			input.isKeyDown(KEY_KEY_D);

		if (movementInput)
			playerRotY = cameraYaw;

		if (!movementInput && input.consumeRightClick())
			playerRotY = cameraYaw;

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

		if (moveDir.getLength() > 0)
			moveDir.normalize();

		if (isShooting)
		{
			moveDir = vector3df(0, 0, 0);
			moving = false;
		}

		playerPos += moveDir * PLAYER_SPEED * deltaTime;

		shootCooldown -= deltaTime;
		if (shootCooldown < 0) shootCooldown = 0;

		if (attackAnimTimer > 0)
		{
			attackAnimTimer -= deltaTime;
			if (attackAnimTimer <= 0)
			{
				isShooting = false;
				if (playerNode)
				{
					if (moving)
					{
						playerNode->setMD2Animation(EMAT_RUN);
						if (weaponNode) weaponNode->setMD2Animation(EMAT_RUN);
					}
					else
					{
						playerNode->setMD2Animation(EMAT_STAND);
						if (weaponNode) weaponNode->setMD2Animation(EMAT_STAND);
					}
				}
			}
		}

		if (input.isLeftMouseDown() && shootCooldown <= 0)
		{
			shootCooldown = GUN_FIRE_RATE;
			attackAnimTimer = ATTACK_ANIM_DURATION;
			isShooting = true;

			if (playerNode)
			{
				playerNode->setMD2Animation(EMAT_ATTACK);
				if (weaponNode) weaponNode->setMD2Animation(EMAT_ATTACK);
			}

			vector3df rayStart = playerPos + vector3df(0, 30, 0);
			vector3df rayEnd = rayStart + forward * SHOOT_RANGE;

			line3d<f32> ray(rayStart, rayEnd);
			ISceneCollisionManager* collMan = smgr->getSceneCollisionManager();

			vector3df hitPoint;
			triangle3df hitTriangle;
			ISceneNode* hitNode = collMan->getSceneNodeAndCollisionPointFromRay(
				ray, hitPoint, hitTriangle, 0, 0);

			if (hitNode && hitNode != playerNode && hitNode != ground)
			{
			}
		}

		if (playerNode)
		{
			playerNode->setPosition(playerPos);
			playerNode->setRotation(vector3df(0, playerRotY + MD2_ROTATION_OFFSET, 0));

			if (!isShooting)
			{
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
			}
			isMoving = moving;
		}

		f32 camYawRad = cameraYaw * core::DEGTORAD;
		vector3df camForward(sinf(camYawRad), 0, cosf(camYawRad));
		vector3df camOffset = -camForward * CAMERA_DISTANCE + vector3df(0, CAMERA_HEIGHT, 0);
		vector3df camPos = playerPos + camOffset;
		vector3df camTarget = playerPos + vector3df(0, 30, 0);

		camera->setPosition(camPos);
		camera->setTarget(camTarget);

		driver->beginScene(true, true, SColor(255, 100, 130, 180));
		smgr->drawAll();
		driver->endScene();

		if (input.isKeyDown(KEY_ESCAPE))
			device->closeDevice();
	}

	device->drop();
	return 0;
}
