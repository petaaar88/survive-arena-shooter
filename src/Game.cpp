#include "Game.h"
#include <cmath>


static const f32 MOUSE_SENSITIVITY = 0.2f;
static const f32 CAMERA_DISTANCE = 120.0f;
static const f32 CAMERA_HEIGHT = 30.0f;

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
	, m_ammoPickup(nullptr)
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
	delete m_ammoPickup;
	m_ammoPickup = nullptr;

	for (Enemy* e : m_enemies) delete e;
	m_enemies.clear();

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
	m_device = createDevice(video::EDT_OPENGL, dimension2d<u32>(1300, 780), 16,
		false, false, false, &m_input);

	if (!m_device)
		return;

	m_device->setWindowCaption(L"Survive - Arena Shooter");
	m_device->getCursorControl()->setVisible(false);

	m_driver = m_device->getVideoDriver();
	m_smgr = m_device->getSceneManager();
	m_gui = m_device->getGUIEnvironment();

	IGUISkin* skin = m_gui->getSkin();
	IGUIFont* font = m_gui->getFont("assets/textures/fonts/bigfont.png");
	if (font)
		skin->setFont(font);

	// Create physics world before scene objects
	m_physics = new Physics();

	// Debug drawer for physics visualization
	m_debugDrawer = new DebugDrawer(m_driver);
	m_physics->setDebugDrawer(m_debugDrawer);

	setupScene();
	setupHUD();

	m_player = new Player(m_smgr, m_driver, m_physics);
	m_enemies.push_back(new Enemy(m_smgr, m_driver, m_physics, vector3df(200, 0, 200)));
	m_enemies.push_back(new Enemy(m_smgr, m_driver, m_physics, vector3df(-200, 0, -200)));
	m_enemies.push_back(new Enemy(m_smgr, m_driver, m_physics, vector3df(-200, 0, 200)));
	m_enemies.push_back(new Enemy(m_smgr, m_driver, m_physics, vector3df(200, 0, -200)));
	m_enemies.push_back(new Enemy(m_smgr, m_driver, m_physics, vector3df(300, 0, 0)));
	m_enemies.push_back(new Enemy(m_smgr, m_driver, m_physics, vector3df(-300, 0, 0)));
	m_ammoPickup = new Pickup(m_smgr, m_driver, m_physics, vector3df(-100, -25, -100), PickupType::AMMO);

	m_lastTime = m_device->getTimer()->getTime();

	dimension2d<u32> screenSize = m_driver->getScreenSize();
	m_centerX = screenSize.Width / 2;
	m_centerY = screenSize.Height / 2;
	m_device->getCursorControl()->setPosition(m_centerX, m_centerY);


	// Obstacle data: { posX, posZ, width, height, depth, isPillar }
	struct ObstacleData { float x, z, w, h, d; bool isPillar; };
	static const ObstacleData obstacles[] =
	{
		// Pillars (tall & thick)
		{  150,  250, 50, 180, 50, true },
		{ -300,  400, 60, 210, 60, true },
		{  500, -200, 40, 150, 40, true },
		{ -600, -500, 70, 240, 70, true },
		{  800,  600, 50, 165, 50, true },
		{ -900,  100, 60, 195, 60, true },
		{  350, -700, 40, 225, 40, true },
		{ -200, -900, 50, 180, 50, true },
		{ 1000, -400, 60, 210, 60, true },
		{-1100,  700, 50, 150, 50, true },
		{  700,  900, 70, 240, 70, true },
		{ -500,  800, 40, 165, 40, true },

		// Boxes (short & wide)
		{  250, -350, 50, 30, 40, false },
		{ -400, -150, 60, 40, 50, false },
		{  600,  350, 40, 25, 35, false },
		{ -750,  500, 55, 45, 45, false },
		{  450,  700, 45, 30, 55, false },
		{ -300, -600, 50, 40, 40, false },
		{  900, -100, 35, 25, 60, false },
		{-1000, -300, 60, 45, 50, false },
		{  200,  500, 40, 30, 40, false },
		{ -150,  150, 55, 40, 55, false },
		{  750, -800, 45, 25, 35, false },
		{ -800, -700, 50, 45, 45, false },
	};

	static const float groundY = -25.0f;
	ITexture* pillarTex = m_driver->getTexture("assets/textures/obstacles/pillar.png");
	ITexture* boxTex = m_driver->getTexture("assets/textures/obstacles/box.png");

	for (const auto& obs : obstacles)
	{
		scene::ISceneNode* node = m_smgr->addCubeSceneNode(1.0f);
		if (node)
		{
			node->setScale(core::vector3df(obs.w, obs.h, obs.d));

			core::vector3df pos(obs.x, groundY + obs.h / 2.0f, obs.z);
			node->setPosition(pos);
			node->setMaterialFlag(video::EMF_LIGHTING, false);
			node->setMaterialTexture(0, obs.isPillar ? pillarTex : boxTex);

			btBoxShape* shape = new btBoxShape(btVector3(obs.w * 0.5f, obs.h * 0.5f, obs.d * 0.5f));
			m_physics->createRigidBody(0.0f, shape, pos);
		}
	}
}

void Game::setupScene()
{
	// Camera
	m_camera = m_smgr->addCameraSceneNode();

	// Ground plane (visual)
	m_ground = m_smgr->addCubeSceneNode(10.0f);
	if (m_ground)
	{
		m_ground->setScale(vector3df(300.0f, 0.1f, 300.0f));
		m_ground->setPosition(vector3df(0, -25, 0));
		m_ground->setMaterialFlag(EMF_LIGHTING, false);
		m_ground->setMaterialTexture(0, m_driver->getTexture("assets/textures/building/ground.jpg"));
	}

	// Ground physics body (static box)
	btBoxShape* groundShape = new btBoxShape(btVector3(1500.0f, 0.5f, 1500.0f));
	m_groundBody = m_physics->createRigidBody(0.0f, groundShape, vector3df(0, -25, 0));

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
		IGUIImage* img = m_gui->addImage(bulletHudGui, position2d<s32>(m_driver->getScreenSize().Width - (m_driver->getScreenSize().Width / 7), m_driver->getScreenSize().Height - (m_driver->getScreenSize().Height / 8)));
		img->setScaleImage(true);
		img->setMaxSize(dimension2du(72,72));
	}

	// Ammo counter
	m_ammoText = m_gui->addStaticText(
		L"5",
		rect<s32>(m_driver->getScreenSize().Width - (m_driver->getScreenSize().Width / 11), m_driver->getScreenSize().Height - (m_driver->getScreenSize().Height / 10), m_driver->getScreenSize().Width - (m_driver->getScreenSize().Width / 10) + 200, 900),
		false, false, 0, -1, false
	);
	m_ammoText->setOverrideColor(SColor(255, 255, 255, 255));

	// Health display
	m_healthText = m_gui->addStaticText(
		L"HP: 100",
		rect<s32>(m_driver->getScreenSize().Width / 22, m_driver->getScreenSize().Height - (m_driver->getScreenSize().Height / 10), 400, 900),
		false, false, 0, -1, false
	);
	m_healthText->setOverrideColor(SColor(255, 255, 255, 255));

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
		crosshair = m_gui->addImage(crosshairTex, position2d<s32>(cx, cy));
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

	// 1. Handle player input (movement + shooting)
	m_player->handleInput(deltaTime, m_input, m_cameraYaw);

	// 2. Determine which enemy gets attack permission (only one at a time)
	bool anyAttacking = false;
	for (Enemy* enemy : m_enemies)
	{
		if (!enemy->isDead() && enemy->getState() == EnemyState::ATTACK)
		{
			enemy->setAttackAllowed(true);
			anyAttacking = true;
		}
		else
		{
			enemy->setAttackAllowed(false);
		}
	}
	if (!anyAttacking)
	{
		Enemy* closest = nullptr;
		f32 closestDist = 999999.0f;
		vector3df playerPos = m_player->getPosition();
		for (Enemy* enemy : m_enemies)
		{
			if (!enemy->isDead()
				&& (enemy->getState() == EnemyState::WAIT_ATTACK
					|| enemy->getState() == EnemyState::CHASE))
			{
				f32 dist = enemy->getPosition().getDistanceFrom(playerPos);
				if (dist < closestDist)
				{
					closestDist = dist;
					closest = enemy;
				}
			}
		}
		if (closest)
			closest->setAttackAllowed(true);
	}

	// Allow saluting for all non-dead chasing enemies
	for (Enemy* enemy : m_enemies)
		enemy->setSaluteAllowed(!enemy->isDead() && enemy->getState() == EnemyState::CHASE);

	// Update enemy AI (sets velocities)
	for (Enemy* enemy : m_enemies)
		enemy->updateAI(deltaTime, m_player->getPosition());

	// 3. Step physics simulation
	m_physics->stepSimulation(deltaTime);

	// 4. Sync physics → visual nodes
	m_player->update(deltaTime);
	for (Enemy* enemy : m_enemies)
		enemy->update(deltaTime);

	// 5. Check if player's shot hit any enemy
	btCollisionObject* hitObject = m_player->getLastHitObject();
	if (hitObject)
	{
		for (Enemy* enemy : m_enemies)
		{
			GameObject* hitGameObject = static_cast<GameObject*>(hitObject->getUserPointer());
			if (hitGameObject == enemy)
			{
				enemy->takeDamage(25);
				break;
			}
		}
	}

	// 6. Check if any enemy's attack trigger overlaps player
	for (Enemy* enemy : m_enemies)
	{
		if (!enemy->isDead()
			&& enemy->getAttackTrigger() && m_player->getBody()
			&& m_physics->isGhostOverlapping(enemy->getAttackTrigger(), m_player->getBody())
			&& enemy->wantsToDealDamage())
		{
			m_player->takeDamage(enemy->getAttackDamage());
			enemy->resetAttackCooldown();
		}
	}

	// 7. Check ammo pickup overlap
	if (m_ammoPickup)
	{
		m_ammoPickup->update(deltaTime);
		if (!m_ammoPickup->isCollected()
			&& m_ammoPickup->getTrigger() && m_player->getBody()
			&& m_physics->isGhostOverlapping(m_ammoPickup->getTrigger(), m_player->getBody()))
		{
			m_player->addAmmo(30);
			m_ammoPickup->collect();
		}
	}

	// Remove dead enemies after death animation finishes
	for (auto it = m_enemies.begin(); it != m_enemies.end(); )
	{
		if ((*it)->shouldRemove())
		{
			delete *it;
			it = m_enemies.erase(it);
		}
		else
		{
			++it;
		}
	}

	updateCamera();
	updateHUD();

	// Check for death → transition to GAMEOVER (after HUD update so HP shows 0)
	if (m_player->isDead())
	{
		m_state = GameState::GAMEOVER;
		return;
	}

	if (crosshair)
	{
		bool showCrosshair = m_input.isRightMouseDown();

		crosshair->setVisible(showCrosshair);
	}
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
