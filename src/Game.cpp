#include "Game.h"
#include <cmath>

static const f32 MOUSE_SENSITIVITY = 0.2f;
static const f32 CAMERA_DISTANCE = 100.0f;
static const f32 CAMERA_HEIGHT = 80.0f;

Game::Game()
	: m_device(nullptr)
	, m_driver(nullptr)
	, m_smgr(nullptr)
	, m_gui(nullptr)
	, m_physics(nullptr)
	, m_debugDrawer(nullptr)
	, m_groundBody(nullptr)
	, m_showDebug(false)
	, m_player(nullptr)
	, m_enemy(nullptr)
	, m_camera(nullptr)
	, m_ground(nullptr)
	, m_ammoText(nullptr)
	, m_healthText(nullptr)
	, m_state(GameState::PLAYING)
	, m_cameraYaw(0.0f)
	, m_lastTime(0)
	, m_centerX(0)
	, m_centerY(0)
{
	init();
}

Game::~Game()
{
	delete m_enemy;
	m_enemy = nullptr;

	delete m_player;
	m_player = nullptr;

	delete m_debugDrawer;
	m_debugDrawer = nullptr;

	delete m_physics;
	m_physics = nullptr;

	if (m_device)
		m_device->drop();
}

void Game::init()
{
	m_device = createDevice(video::EDT_OPENGL, dimension2d<u32>(800, 600), 16,
		false, false, false, &m_input);

	if (!m_device)
		return;

	m_device->setWindowCaption(L"Survive - Arena Shooter");
	m_device->getCursorControl()->setVisible(false);

	m_driver = m_device->getVideoDriver();
	m_smgr = m_device->getSceneManager();
	m_gui = m_device->getGUIEnvironment();

	// Create physics world before scene objects
	m_physics = new Physics();

	// Debug drawer for physics visualization
	m_debugDrawer = new DebugDrawer(m_driver);
	m_physics->setDebugDrawer(m_debugDrawer);

	setupScene();
	setupHUD();

	m_player = new Player(m_smgr, m_driver, m_physics);
	m_enemy = new Enemy(m_smgr, m_driver, m_physics, vector3df(200, 0, 200));

	m_lastTime = m_device->getTimer()->getTime();

	dimension2d<u32> screenSize = m_driver->getScreenSize();
	m_centerX = screenSize.Width / 2;
	m_centerY = screenSize.Height / 2;
	m_device->getCursorControl()->setPosition(m_centerX, m_centerY);
}

void Game::setupScene()
{
	// Camera
	m_camera = m_smgr->addCameraSceneNode();

	// Ground plane (visual)
	m_ground = m_smgr->addCubeSceneNode(10.0f);
	if (m_ground)
	{
		m_ground->setScale(vector3df(100.0f, 0.1f, 100.0f));
		m_ground->setPosition(vector3df(0, -5, 0));
		m_ground->setMaterialFlag(EMF_LIGHTING, false);
		m_ground->setMaterialTexture(0, m_driver->getTexture("assets/models/player/caleb.pcx"));
	}

	// Ground physics body (static box)
	btBoxShape* groundShape = new btBoxShape(btVector3(500.0f, 0.5f, 500.0f));
	m_groundBody = m_physics->createRigidBody(0.0f, groundShape, vector3df(0, -5, 0));

	// Lighting
	m_smgr->addLightSceneNode(0, vector3df(0, 200, 0), SColorf(1.0f, 1.0f, 1.0f), 800.0f);
	m_smgr->setAmbientLight(SColorf(0.3f, 0.3f, 0.3f));
}

void Game::setupHUD()
{
	// Bullet icon
	ITexture* bulletHudGui = m_driver->getTexture("assets/textures/hud/bullet_icon.png");
	if (bulletHudGui)
	{
		IGUIImage* img = m_gui->addImage(bulletHudGui, position2d<s32>(710, 440));
		img->setScaleImage(true);
		img->setMaxSize(dimension2du(52, 52));
	}

	// Ammo counter
	m_ammoText = m_gui->addStaticText(
		L"5",
		rect<s32>(765, 448, 800, 488),
		false, false, 0, -1, false
	);
	m_ammoText->setOverrideColor(SColor(255, 255, 255, 255));

	// Health display
	m_healthText = m_gui->addStaticText(
		L"HP: 100",
		rect<s32>(10, 10, 200, 40),
		false, false, 0, -1, false
	);
	m_healthText->setOverrideColor(SColor(255, 255, 50, 50));

	// Crosshair
	ITexture* crosshairTex = m_driver->getTexture("assets/textures/hud/crosshair.png");
	if (crosshairTex)
	{
		dimension2d<u32> screenSize = m_driver->getScreenSize();
		s32 screenCX = screenSize.Width / 2;
		s32 screenCY = screenSize.Height / 2;
		s32 crosshairSize = 32;
		s32 cx = screenCX - crosshairSize / 2;
		s32 cy = screenCY - crosshairSize / 2;
		IGUIImage* crosshair = m_gui->addImage(crosshairTex, position2d<s32>(cx, cy));
		crosshair->setScaleImage(true);
		crosshair->setMaxSize(dimension2du(crosshairSize, crosshairSize));
	}
	
}

void Game::run()
{
	if (!m_device)
		return;

	while (m_device->run())
	{
		if (!m_device->isWindowActive())
		{
			m_device->yield();
			continue;
		}

		// Delta time
		u32 currentTime = m_device->getTimer()->getTime();
		f32 deltaTime = (currentTime - m_lastTime) / 1000.0f;
		m_lastTime = currentTime;
		if (deltaTime > 0.1f) deltaTime = 0.1f;

		// Update based on state
		switch (m_state)
		{
		case GameState::PLAYING:
			updatePlaying(deltaTime);
			break;
		case GameState::GAMEOVER:
			updateGameOver(deltaTime);
			break;
		case GameState::MENU:
		case GameState::WIN:
			break;
		}

		// Toggle debug visualization
		if (m_input.consumeKeyPress(KEY_F1))
			m_showDebug = !m_showDebug;

		// Render
		m_driver->beginScene(true, true, SColor(0, 0, 0, 0));
		m_smgr->drawAll();

		// Debug: draw physics colliders and shooting ray
		if (m_showDebug)
		{
			m_debugDrawer->beginDraw();
			m_physics->debugDrawWorld();

			const auto& ray = m_player->getDebugRay();
			if (ray.active)
			{
				video::SMaterial lineMat;
				lineMat.Lighting = false;
				lineMat.Thickness = 13.0f;
				m_driver->setMaterial(lineMat);
				m_driver->draw3DLine(ray.start, ray.end, SColor(255, 0, 100, 255));
			}
		}

		m_gui->drawAll();
		m_driver->endScene();

		if (m_input.isKeyDown(KEY_ESCAPE))
			m_device->closeDevice();
	}
}

void Game::updatePlaying(f32 deltaTime)
{
	// Mouse look
	position2d<s32> cursorPos = m_device->getCursorControl()->getPosition();
	f32 mouseDX = (f32)(cursorPos.X - m_centerX);
	m_cameraYaw += mouseDX * MOUSE_SENSITIVITY;
	m_device->getCursorControl()->setPosition(m_centerX, m_centerY);

	// Test: T key deals damage to player
	if (m_input.consumeKeyPress(KEY_KEY_T))
		m_player->takeDamage(25);

	// 1. Handle player input (movement + shooting)
	m_player->handleInput(deltaTime, m_input, m_cameraYaw);

	// 2. Update enemy AI (sets velocities)
	if (m_enemy)
		m_enemy->updateAI(deltaTime, m_player->getPosition());

	// 3. Step physics simulation
	m_physics->stepSimulation(deltaTime);

	// 4. Sync physics → visual nodes
	m_player->update(deltaTime);
	if (m_enemy)
		m_enemy->update(deltaTime);

	// 5. Check if player's shot hit the enemy
	btCollisionObject* hitObject = m_player->getLastHitObject();
	if (hitObject && m_enemy)
	{
		GameObject* hitGameObject = static_cast<GameObject*>(hitObject->getUserPointer());
		if (hitGameObject == m_enemy)
			m_enemy->takeDamage(25);
	}

	// Check for death → transition to GAMEOVER
	if (m_player->isDead())
	{
		m_state = GameState::GAMEOVER;
		return;
	}

	updateCamera();
	updateHUD();
}

void Game::updateGameOver(f32 deltaTime)
{
	// Camera stays fixed on dead player, no mouse look
}

void Game::updateCamera()
{
	vector3df playerPos = m_player->getPosition();
	f32 camYawRad = m_cameraYaw * core::DEGTORAD;
	vector3df camForward(sinf(camYawRad), 0, cosf(camYawRad));
	vector3df camOffset = -camForward * CAMERA_DISTANCE + vector3df(0, CAMERA_HEIGHT, 0);

	m_camera->setPosition(playerPos + camOffset);
	m_camera->setTarget(playerPos + vector3df(0, 30, 0));
}

void Game::updateHUD()
{
	wchar_t ammoStr[16];
	swprintf(ammoStr, 16, L"%d", m_player->getAmmo());
	m_ammoText->setText(ammoStr);

	wchar_t hpStr[32];
	swprintf(hpStr, 32, L"HP: %d", m_player->getHealth());
	m_healthText->setText(hpStr);
}
