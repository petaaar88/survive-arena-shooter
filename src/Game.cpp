#include "Game.h"
#include <cmath>
#include <iostream>

static const f32 MOUSE_SENSITIVITY = 0.2f;
static const f32 CAMERA_DISTANCE = 120.0f;
static const f32 CAMERA_HEIGHT = 30.0f;
static const f32 ENEMY_CHASING_VOLUME = 0.2f;

// Wave system
static const f32 GAME_DURATION = 180.0f;
static const f32 WAVE1_TIME = 120.0f;  // wave 1 while timer > 120
static const f32 WAVE2_TIME = 60.0f;   // wave 2 while timer > 60
static const s32 WAVE1_MAX = 3;
static const s32 WAVE2_MAX = 6;
static const s32 WAVE3_MAX = 10;
static const f32 WAVE1_SPAWN_INTERVAL = 5.0f;
static const f32 WAVE2_SPAWN_INTERVAL = 2.0f;
static const f32 WAVE3_SPAWN_INTERVAL = 1.0f;

// Pickup system
static const f32 PICKUP_SPAWN_MIN = 8.0f;
static const f32 PICKUP_SPAWN_MAX = 20.0f;
static const s32 PICKUP_AMMO_AMOUNT = 7;

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
	, m_pickupSpawnTimer(0.0f)
	, m_camera(nullptr)
	, m_ground(nullptr)
	, m_ammoText(nullptr)
	, m_healthText(nullptr)
	, m_timerText(nullptr)
	, m_waveText(nullptr)
	, m_killText(nullptr)
	, m_killCount(0)
	, m_state(GameState::TESTING)
	, m_menuBgTex(nullptr)
	, m_logoTex(nullptr)
	, m_playBtnTex(nullptr)
	, m_creditsBtnTex(nullptr)
	, m_exitBtnTex(nullptr)
	, m_resumeBtnTex(nullptr)
	, m_cameraYaw(0.0f)
	, m_lastTime(0)
	, m_centerX(0)
	, m_centerY(0)
	, m_soundEngine(nullptr)
	, m_chasingSound(nullptr)
	, m_gameTimer(GAME_DURATION)
	, m_spawnTimer(0.0f)
	, m_currentWave(1)
{
	m_soundEngine = irrklang::createIrrKlangDevice();
	init();
}

Game::~Game()
{
	if (m_chasingSound)
	{
		m_chasingSound->stop();
		m_chasingSound->drop();
		m_chasingSound = nullptr;
	}
	if (m_soundEngine)
	{
		m_soundEngine->drop();
		m_soundEngine = nullptr;
	}

	for (Pickup* p : m_pickups) delete p;
	m_pickups.clear();

	for (Enemy* e : m_enemies) delete e;
	m_enemies.clear();

	for (FogEnemy* f : m_fogEnemies) delete f;
	m_fogEnemies.clear();

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
	m_device = createDevice(video::EDT_DIRECT3D9, dimension2d<u32>(1300, 780), 16,
		false, false, false, &m_input);

	if (!m_device)
		return;

	m_device->setWindowCaption(L"Survive - Arena Shooter");
	m_device->getCursorControl()->setVisible(m_state != GameState::TESTING);

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
	//m_enemies.push_back(new Enemy(m_smgr, m_driver, m_physics, vector3df(200, 0, 200)));
	//m_enemies.push_back(new Enemy(m_smgr, m_driver, m_physics, vector3df(-200, 0, -200)));
	//m_enemies.push_back(new Enemy(m_smgr, m_driver, m_physics, vector3df(-200, 0, 200)));
	//m_enemies.push_back(new Enemy(m_smgr, m_driver, m_physics, vector3df(200, 0, -200)));
	//m_enemies.push_back(new Enemy(m_smgr, m_driver, m_physics, vector3df(300, 0, 0)));
	//m_enemies.push_back(new Enemy(m_smgr, m_driver, m_physics, vector3df(-300, 0, 0)));
	// Multiple ammo pickup spawn positions
	vector3df pickupPositions[] = {
		vector3df(-400, -25, -300),
		vector3df( 400, -25,  300),
		vector3df(-400, -25,  300),
		vector3df( 400, -25, -300),
		vector3df(   0, -25,    0),
		vector3df( 500, -25,    0),
	};

	for (const auto& pos : pickupPositions)
	{
		Pickup* p = new Pickup(m_smgr, m_driver, m_physics, pos, PickupType::AMMO);
		p->setCollected(true); // start hidden
		m_pickups.push_back(p);
	}
	m_pickupSpawnTimer = PICKUP_SPAWN_MIN + static_cast<f32>(rand()) / RAND_MAX * (PICKUP_SPAWN_MAX - PICKUP_SPAWN_MIN);

	// Load menu textures
	m_menuBgTex = m_driver->getTexture("assets/textures/backgrounds/mainmenu.png");
	m_logoTex = m_driver->getTexture("assets/textures/UI/logo.png");
	m_playBtnTex = m_driver->getTexture("assets/textures/UI/play.png");
	m_creditsBtnTex = m_driver->getTexture("assets/textures/UI/credits.png");
	m_exitBtnTex = m_driver->getTexture("assets/textures/UI/exit.png");
	m_resumeBtnTex = m_driver->getTexture("assets/textures/UI/resume.png");

	// Calculate button rects (centered on screen)
	{
		dimension2d<u32> ss = m_driver->getScreenSize();
		s32 btnW = 200, btnH = 60, spacing = 20;
		s32 cx = ss.Width / 2;

		// Logo at top area
		s32 logoW = 400, logoH = 150;
		s32 logoY = 120;
		m_logoRect = rect<s32>(cx - logoW/2, logoY, cx + logoW/2, logoY + logoH);

		// Play and Credits grouped together below logo
		s32 groupStartY = logoY + logoH + 40;
		m_playBtnRect = rect<s32>(cx - btnW/2, groupStartY, cx + btnW/2, groupStartY + btnH);
		m_creditsBtnRect = rect<s32>(cx - btnW/2, groupStartY + btnH + spacing, cx + btnW/2, groupStartY + 2*btnH + spacing);

		// Exit button lower, separated from the group
		s32 exitY = ss.Height - btnH - 60;
		m_exitBtnRect = rect<s32>(cx - btnW/2, exitY, cx + btnW/2, exitY + btnH);

		// Pause menu buttons
		s32 pauseStartY = ss.Height / 2 - (2 * btnH + spacing) / 2;
		m_resumeBtnRect = rect<s32>(cx - btnW/2, pauseStartY, cx + btnW/2, pauseStartY + btnH);
		m_pauseExitBtnRect = rect<s32>(cx - btnW/2, pauseStartY + btnH + spacing, cx + btnW/2, pauseStartY + 2*btnH + spacing);

		// Game over / win exit button (below center)
		s32 endExitY = ss.Height / 2 + 80;
		m_endScreenExitBtnRect = rect<s32>(cx - btnW/2, endExitY, cx + btnW/2, endExitY + btnH);
	}

	// Hide HUD initially unless starting in TESTING
	setHUDVisible(m_state == GameState::TESTING);

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
	ITexture* boxTex = m_driver->getTexture("assets/textures/obstacles/box.jpg");

	for (const auto& obs : obstacles)
	{
		scene::ISceneNode* node = m_smgr->addCubeSceneNode(1.0f);
		if (node)
		{
			node->setScale(core::vector3df(obs.w, obs.h, obs.d));

			core::vector3df pos(obs.x, groundY + obs.h / 2.0f, obs.z);
			node->setPosition(pos);
			node->setMaterialFlag(video::EMF_LIGHTING, false);
			node->setMaterialFlag(video::EMF_FOG_ENABLE, true);
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
	m_camera->setFarValue(20000.0f);
	// Ground plane (visual)
	m_ground = m_smgr->addCubeSceneNode(10.0f);
	if (m_ground)
	{
		m_ground->setScale(vector3df(300.0f, 0.1f, 300.0f));
		m_ground->setPosition(vector3df(0, -25, 0));
		m_ground->setMaterialFlag(EMF_LIGHTING, false);
		m_ground->setMaterialFlag(EMF_FOG_ENABLE, true);
		m_ground->setMaterialTexture(0, m_driver->getTexture("assets/textures/building/ground.jpg"));
	}

	m_smgr->addSkyBoxSceneNode(
		m_driver->getTexture("assets/textures/skybox/irrlicht2_up.jpg"), 
		m_driver->getTexture("assets/textures/skybox/irrlicht2_dn.jpg"), 
		m_driver->getTexture("assets/textures/skybox/irrlicht2_lf.jpg"),
		m_driver->getTexture("assets/textures/skybox/irrlicht2_rt.jpg"),
		m_driver->getTexture("assets/textures/skybox/irrlicht2_ft.jpg"),
		m_driver->getTexture("assets/textures/skybox/irrlicht2_bk.jpg"));

	// Ground physics body (static box)
	btBoxShape* groundShape = new btBoxShape(btVector3(1500.0f, 0.5f, 1500.0f));
	m_groundBody = m_physics->createRigidBody(0.0f, groundShape, vector3df(0, -25, 0));

	// Arena boundary walls (invisible)
	float wallHeight = 200.0f;
	float wallThickness = 80.0f;
	float halfGround = 1500.0f;
	float wallY = -25.0f + wallHeight / 2.0f;

	// +X wall at x=1500
	btBoxShape* wallShapeX = new btBoxShape(btVector3(wallThickness / 2.0f, wallHeight / 2.0f, halfGround));
	m_physics->createRigidBody(0.0f, wallShapeX, vector3df(halfGround - 300, wallY, 0));
	// -X wall at x=-1500
	m_physics->createRigidBody(0.0f, new btBoxShape(btVector3(wallThickness / 2.0f, wallHeight / 2.0f, halfGround)), vector3df(-halfGround + 50, wallY, 0));
	// +Z wall at z=1500
	btBoxShape* wallShapeZ = new btBoxShape(btVector3(halfGround, wallHeight / 2.0f, wallThickness / 2.0f));
	m_physics->createRigidBody(0.0f, wallShapeZ, vector3df(0, wallY, halfGround - 300));
	// -Z wall at z=-1500
	m_physics->createRigidBody(0.0f, new btBoxShape(btVector3(halfGround, wallHeight / 2.0f, wallThickness / 2.0f)), vector3df(0, wallY, -halfGround + 320));

	// side wall of +X wall (rotated 45 degrees)
	btBoxShape* wallShapeSide1X = new btBoxShape(btVector3(wallThickness / 2.0f, wallHeight / 2.0f, halfGround));
	btRigidBody* wallSide1X = m_physics->createRigidBody(0.0f, wallShapeSide1X, vector3df(halfGround - 300, wallY, -270));
	btTransform tr;
	wallSide1X->getMotionState()->getWorldTransform(tr);
	tr.setRotation(btQuaternion(btVector3(0, 1, 0), 45.0f * core::DEGTORAD));
	wallSide1X->getMotionState()->setWorldTransform(tr);
	wallSide1X->setWorldTransform(tr);

	// side wall of +X wall (rotated 45 degrees)
	btBoxShape* wallShapeSide2X = new btBoxShape(btVector3(wallThickness / 2.0f, wallHeight / 2.0f, halfGround + 200));
	btRigidBody* wallSide2X = m_physics->createRigidBody(0.0f, wallShapeSide2X, vector3df(halfGround - 50, wallY, 100.0f));
	btTransform tr2;
	wallSide2X->getMotionState()->getWorldTransform(tr2);
	tr2.setRotation(btQuaternion(btVector3(0, 1, 0), -47.0f * core::DEGTORAD));
	wallSide2X->getMotionState()->setWorldTransform(tr2);
	wallSide2X->setWorldTransform(tr2);

	// Lighting
	m_smgr->addLightSceneNode(0, vector3df(0, 200, 0), SColorf(1.0f, 1.0f, 1.0f), 800.0f);
	m_smgr->setAmbientLight(SColorf(0.3f, 0.3f, 0.3f));


	IMeshSceneNode* map = m_smgr->addMeshSceneNode(m_smgr->getMesh("assets/maps/colloseum/Colloseum.obj"));
	map->setPosition(vector3df(-620, 180, 0));

	map->setScale(vector3df(10, 11, 11));
	map->setMaterialFlag(EMF_LIGHTING, false);
	map->setMaterialFlag(EMF_FOG_ENABLE, true);

	// Initialize fog to disabled state
	m_driver->setFog(SColor(255, 180, 180, 180), EFT_FOG_LINEAR, 9999.0f, 10000.0f, 0.0f, false, true);

	setupGates(map);
}

void Game::setupGates(IMeshSceneNode* map)
{
	IMesh* gateMesh = m_smgr->getMesh("assets/maps/gate/gate.obj");
	ITexture* pillarTex = m_driver->getTexture("assets/textures/obstacles/pillar.png");

	struct GateData { vector3df position; f32 rotationY; };
	GateData gates[] =
	{
		{ vector3df( -7, -25,  -5),  90.0f },  // +X edge, face inward
		{ vector3df( 105, -25,  5), -90.0f },  // -X edge, face inward
		{ vector3df(    0, -25, 38), 180.0f },  // +Z edge, face inward
		{ vector3df(    0, -25,-38),   0.0f },  // -Z edge, face inward
	};

	for (const auto& g : gates)
	{
		IMeshSceneNode* gate = m_smgr->addMeshSceneNode(gateMesh, map);
		gate->setPosition(g.position);
		gate->setRotation(vector3df(0, g.rotationY, 0));
		gate->setScale(vector3df(4, 3, 8));
		gate->setMaterialFlag(video::EMF_LIGHTING, false);
		gate->setMaterialFlag(video::EMF_FOG_ENABLE, true);

		// Black backdrop
		IMeshSceneNode* blackCube = m_smgr->addCubeSceneNode(20.0f, gate);
		blackCube->setPosition(vector3df(2, 5.6f, -10.3f));
		blackCube->setScale(vector3df(0.3f, 0.35f, 0.1f));
		blackCube->setMaterialFlag(video::EMF_LIGHTING, true);
		blackCube->setMaterialFlag(video::EMF_FOG_ENABLE, true);
		blackCube->setMaterialType(video::EMT_SOLID);
		blackCube->getMaterial(0).ColorMaterial = video::ECM_NONE;
		blackCube->getMaterial(0).DiffuseColor = SColor(255, 0, 0, 0);
		blackCube->getMaterial(0).AmbientColor = SColor(255, 0, 0, 0);
		blackCube->getMaterial(0).EmissiveColor = SColor(255, 0, 0, 0);

		// Left pillar
		IMeshSceneNode* leftCube = m_smgr->addCubeSceneNode(20.0f, gate);
		leftCube->setPosition(vector3df(-2, 5.6f, -9.8f));
		leftCube->setScale(vector3df(0.06f, 0.35f, 0.1f));
		leftCube->setMaterialFlag(video::EMF_LIGHTING, false);
		leftCube->setMaterialFlag(video::EMF_FOG_ENABLE, true);
		leftCube->setMaterialTexture(0, pillarTex);

		// Right pillar
		IMeshSceneNode* rightCube = m_smgr->addCubeSceneNode(20.0f, gate);
		rightCube->setPosition(vector3df(5.5f, 5.6f, -9.8f));
		rightCube->setScale(vector3df(0.06f, 0.35f, 0.1f));
		rightCube->setMaterialFlag(video::EMF_LIGHTING, false);
		rightCube->setMaterialFlag(video::EMF_FOG_ENABLE, true);
		rightCube->setMaterialTexture(0, pillarTex);

		// Convert local gate position to world space
		vector3df mapPos = map->getPosition();
		vector3df mapScale = map->getScale();
		vector3df worldPos(
			mapPos.X + g.position.X * mapScale.X,
			0.0f,  // above ground (ground is at -25)
			mapPos.Z + g.position.Z * mapScale.Z
		);
		m_gatePositions.push_back(worldPos);
	}
}

void Game::spawnEnemyAtGate(int gateIndex, EnemyType type)
{
	if (gateIndex < 0 || gateIndex >= (int)m_gatePositions.size())
		return;

	static const f32 SPAWN_OFFSET = 700.0f; // how far behind the gate the enemy spawns

	vector3df gatePos = m_gatePositions[gateIndex];
	vector3df forward = vector3df(0, 0, 0) - gatePos;
	forward.Y = 0;
	forward.normalize();

	// Spawn behind the gate, enemy walks forward through it
	vector3df spawnPos = gatePos - forward * SPAWN_OFFSET;
	spawnPos.Y = 0.0f;

	// Per-gate spawn position adjustments
	if (gateIndex == 2)
	{
		spawnPos.X += 500.0f;               // shift +X
		spawnPos.Z += 400.0f;               // shift backward (+Z)
		forward = vector3df(0, 0, -1);      // face -Z
	}
	if (gateIndex == 3)
	{
		spawnPos.X += 700.0f;               // shift +X
		spawnPos.Z -= 400.0f;               // shift backward (-Z)
		forward = vector3df(0, 0, 1);       // face +Z
	}

	m_enemies.push_back(new Enemy(m_smgr, m_driver, m_physics, spawnPos, forward, m_soundEngine, type));
}

void Game::spawnFogEnemyAtGate(int gateIndex)
{
	if (gateIndex < 0 || gateIndex >= (int)m_gatePositions.size())
		return;

	static const f32 SPAWN_OFFSET = 700.0f;

	vector3df gatePos = m_gatePositions[gateIndex];
	vector3df forward = vector3df(0, 0, 0) - gatePos;
	forward.Y = 0;
	forward.normalize();

	vector3df spawnPos = gatePos - forward * SPAWN_OFFSET;
	spawnPos.Y = 0.0f;

	if (gateIndex == 2)
	{
		spawnPos.X += 500.0f;
		spawnPos.Z += 400.0f;
		forward = vector3df(0, 0, -1);
	}
	if (gateIndex == 3)
	{
		spawnPos.X += 700.0f;
		spawnPos.Z -= 400.0f;
		forward = vector3df(0, 0, 1);
	}

	m_fogEnemies.push_back(new FogEnemy(m_smgr, m_driver, m_physics, spawnPos, forward, m_soundEngine));
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

	// Timer (top-center)
	{
		s32 screenW = m_driver->getScreenSize().Width;
		m_timerText = m_gui->addStaticText(
			L"3:00",
			rect<s32>(screenW / 2 - 60, 30, screenW / 2 + 60, 70),
			false, false, 0, -1, false
		);
		m_timerText->setOverrideColor(SColor(255, 255, 255, 255));
	}

	// Wave indicator (below timer)
	{
		s32 screenW = m_driver->getScreenSize().Width;
		m_waveText = m_gui->addStaticText(
			L"Wave 1",
			rect<s32>(screenW / 2 - 60, 80, screenW / 2 + 60, 115),
			false, false, 0, -1, false
		);
		m_waveText->setOverrideColor(SColor(255, 255, 200, 0));
	}

	// Kill count (top-right)
	{
		s32 screenW = m_driver->getScreenSize().Width;
		m_killText = m_gui->addStaticText(
			L"Kills: 0",
			rect<s32>(screenW - 250, 30, screenW - 10, 80),
			false, false, 0, -1, false
		);
		m_killText->setOverrideColor(SColor(255, 255, 255, 255));
	}

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
		//std::cout << "X:" << m_player->getPosition().X << ", Y:" << m_player->getPosition().Y << ", Z:" << m_player->getPosition().Z << std::endl;
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
		case GameState::MENU:
			updateMenu();
			break;
		case GameState::PLAYING:
			updatePlaying(deltaTime);
			break;
		case GameState::TESTING:
			updateTesting(deltaTime);
			break;
		case GameState::PAUSED:
			updatePaused();
			break;
		case GameState::CREDITS:
			updateCredits();
			break;
		case GameState::GAMEOVER:
			updateGameOver(deltaTime);
			break;
		case GameState::WIN:
			updateWin(deltaTime);
			break;
		}

		// Toggle debug visualization
		if (m_input.consumeKeyPress(KEY_F1))
			m_showDebug = !m_showDebug;

		// Render — match clear color to fog when active
		SColor clearColor(0, 0, 0, 0);
		for (FogEnemy* f : m_fogEnemies)
		{
			if (f->isFogActive())
			{
				clearColor = SColor(255, 180, 180, 180);
				break;
			}
		}
		m_driver->beginScene(true, true, clearColor);

		if (m_state == GameState::MENU || m_state == GameState::CREDITS)
		{
			// Menu: draw background only (no 3D scene)
			if (m_state == GameState::MENU)
				drawMenu();
			else
				drawCredits();
		}
		else
		{
			// In-game: draw 3D scene
			m_smgr->drawAll();

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

			if (m_state == GameState::PAUSED)
				drawPause();
			else if (m_state == GameState::GAMEOVER)
				drawGameOver();
			else if (m_state == GameState::WIN)
				drawWin();
		}

		m_driver->endScene();
	}
}

void Game::updatePlaying(f32 deltaTime)
{
	// ESC → pause
	if (m_input.consumeKeyPress(KEY_ESCAPE))
	{
		m_state = GameState::PAUSED;
		m_device->getCursorControl()->setVisible(true);
		return;
	}

	// Mouse look
	position2d<s32> cursorPos = m_device->getCursorControl()->getPosition();
	f32 mouseDX = (f32)(cursorPos.X - m_centerX);
	m_cameraYaw += mouseDX * MOUSE_SENSITIVITY;
	m_device->getCursorControl()->setPosition(m_centerX, m_centerY);

	// Test spawn keys (manual, not part of wave system)
	if (m_input.consumeKeyPress(KEY_KEY_1)) spawnEnemyAtGate(0);
	if (m_input.consumeKeyPress(KEY_KEY_2)) spawnEnemyAtGate(1);
	if (m_input.consumeKeyPress(KEY_KEY_3)) spawnEnemyAtGate(2);
	if (m_input.consumeKeyPress(KEY_KEY_4)) spawnEnemyAtGate(3);

	// Wave system: timer and auto-spawning
	m_gameTimer -= deltaTime;
	if (m_gameTimer <= 0.0f)
	{
		m_gameTimer = 0.0f;
		m_state = GameState::WIN;
		m_device->getCursorControl()->setVisible(true);
		setHUDVisible(false);
		if (m_chasingSound)
		{
			m_chasingSound->stop();
			m_chasingSound->drop();
			m_chasingSound = nullptr;
		}
		return;
	}

	// Determine current wave
	s32 maxBasic, maxFast;
	f32 spawnInterval;
	if (m_gameTimer > WAVE1_TIME)
	{
		m_currentWave = 1;
		maxBasic = 3; maxFast = 0;
		spawnInterval = WAVE1_SPAWN_INTERVAL;
	}
	else if (m_gameTimer > WAVE2_TIME)
	{
		m_currentWave = 2;
		maxBasic = 4; maxFast = 2;
		spawnInterval = WAVE2_SPAWN_INTERVAL;
	}
	else
	{
		m_currentWave = 3;
		maxBasic = 4; maxFast = 6;
		spawnInterval = WAVE3_SPAWN_INTERVAL;
	}

	// Count alive enemies by type
	s32 aliveBasic = 0, aliveFast = 0;
	for (Enemy* enemy : m_enemies)
	{
		if (!enemy->isDead())
		{
			if (enemy->getType() == EnemyType::FAST)
				aliveFast++;
			else
				aliveBasic++;
		}
	}

	// Auto-spawn enemies
	m_spawnTimer -= deltaTime;
	if (m_spawnTimer <= 0.0f)
	{
		bool canBasic = aliveBasic < maxBasic;
		bool canFast  = aliveFast < maxFast;
		if (canBasic || canFast)
		{
			EnemyType type;
			if (canBasic && canFast)
				type = (rand() % 2 == 0) ? EnemyType::BASIC : EnemyType::FAST;
			else
				type = canFast ? EnemyType::FAST : EnemyType::BASIC;

			int gateIndex = rand() % (int)m_gatePositions.size();
			spawnEnemyAtGate(gateIndex, type);
			m_spawnTimer = spawnInterval;
		}
		else
		{
			m_spawnTimer = 0.0f;
		}
	}

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

	// Chasing sound: play looping while at least one enemy is chasing
	{
		bool anyChasing = false;
		for (Enemy* enemy : m_enemies)
		{
			if (!enemy->isDead() && enemy->getState() == EnemyState::CHASE && !enemy->isSaluting())
			{
				anyChasing = true;
				break;
			}
		}

		if (anyChasing && m_soundEngine)
		{
			if (!m_chasingSound || m_chasingSound->isFinished())
			{
				if (m_chasingSound) { m_chasingSound->drop(); m_chasingSound = nullptr; }
				m_chasingSound = m_soundEngine->play2D("assets/audio/enemies/chasing.mp3", true, true, true);
				if (m_chasingSound) { m_chasingSound->setVolume(ENEMY_CHASING_VOLUME); m_chasingSound->setIsPaused(false); }
			}
		}
		else if (m_chasingSound)
		{
			m_chasingSound->stop();
			m_chasingSound->drop();
			m_chasingSound = nullptr;
		}
	}

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
				bool wasDead = enemy->isDead();
				enemy->takeDamage(25);
				if (!wasDead && enemy->isDead())
					m_killCount++;
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

	// 7. Pickup spawn timer — randomly respawn a hidden pickup
	m_pickupSpawnTimer -= deltaTime;
	if (m_pickupSpawnTimer <= 0.0f)
	{
		// Collect indices of all hidden pickups
		std::vector<int> hiddenIndices;
		for (int i = 0; i < (int)m_pickups.size(); i++)
		{
			if (m_pickups[i]->isCollected())
				hiddenIndices.push_back(i);
		}
		if (!hiddenIndices.empty())
		{
			int pick = hiddenIndices[rand() % hiddenIndices.size()];
			m_pickups[pick]->respawn();
		}
		m_pickupSpawnTimer = PICKUP_SPAWN_MIN + static_cast<f32>(rand()) / RAND_MAX * (PICKUP_SPAWN_MAX - PICKUP_SPAWN_MIN);
	}

	// 8. Check pickup overlap for all pickups
	for (Pickup* p : m_pickups)
	{
		p->update(deltaTime);
		if (!p->isCollected()
			&& p->getTrigger() && m_player->getBody()
			&& m_physics->isGhostOverlapping(p->getTrigger(), m_player->getBody()))
		{
			m_player->addAmmo(PICKUP_AMMO_AMOUNT);
			p->collect();
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
		m_device->getCursorControl()->setVisible(true);
		setHUDVisible(false);
		if (m_chasingSound)
		{
			m_chasingSound->stop();
			m_chasingSound->drop();
			m_chasingSound = nullptr;
		}
		return;
	}

	if (crosshair)
	{
		bool showCrosshair = m_input.isRightMouseDown();

		crosshair->setVisible(showCrosshair);
	}
}

void Game::updateTesting(f32 deltaTime)
{
	// ESC → back to menu
	if (m_input.consumeKeyPress(KEY_ESCAPE))
	{
		m_state = GameState::MENU;
		m_device->getCursorControl()->setVisible(true);
		setHUDVisible(false);
		return;
	}

	// Mouse look
	position2d<s32> cursorPos = m_device->getCursorControl()->getPosition();
	f32 mouseDX = (f32)(cursorPos.X - m_centerX);
	m_cameraYaw += mouseDX * MOUSE_SENSITIVITY;
	m_device->getCursorControl()->setPosition(m_centerX, m_centerY);

	// Manual spawn: 1-4 = basic enemies, 5-8 = fast enemies
	if (m_input.consumeKeyPress(KEY_KEY_1)) spawnEnemyAtGate(0, EnemyType::BASIC);
	if (m_input.consumeKeyPress(KEY_KEY_2)) spawnEnemyAtGate(1, EnemyType::BASIC);
	if (m_input.consumeKeyPress(KEY_KEY_3)) spawnEnemyAtGate(2, EnemyType::BASIC);
	if (m_input.consumeKeyPress(KEY_KEY_4)) spawnEnemyAtGate(3, EnemyType::BASIC);
	if (m_input.consumeKeyPress(KEY_KEY_5)) spawnEnemyAtGate(0, EnemyType::FAST);
	if (m_input.consumeKeyPress(KEY_KEY_6)) spawnEnemyAtGate(1, EnemyType::FAST);
	if (m_input.consumeKeyPress(KEY_KEY_7)) spawnEnemyAtGate(2, EnemyType::FAST);
	if (m_input.consumeKeyPress(KEY_KEY_8)) spawnEnemyAtGate(3, EnemyType::FAST);
	if (m_input.consumeKeyPress(KEY_KEY_0)) spawnFogEnemyAtGate(rand() % 4);

	// Player input (movement + shooting)
	m_player->handleInput(deltaTime, m_input, m_cameraYaw);

	// Enemy attack permission (same logic as PLAYING)
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

	for (Enemy* enemy : m_enemies)
		enemy->setSaluteAllowed(!enemy->isDead() && enemy->getState() == EnemyState::CHASE);

	// Update enemy AI
	for (Enemy* enemy : m_enemies)
		enemy->updateAI(deltaTime, m_player->getPosition());
	for (FogEnemy* fogEnemy : m_fogEnemies)
		fogEnemy->updateAI(deltaTime, m_player->getPosition());

	// Step physics simulation
	m_physics->stepSimulation(deltaTime);

	// Sync physics → visual nodes
	m_player->update(deltaTime);
	for (Enemy* enemy : m_enemies)
		enemy->update(deltaTime);
	for (FogEnemy* fogEnemy : m_fogEnemies)
		fogEnemy->update(deltaTime);

	// Check if player's shot hit any enemy
	btCollisionObject* hitObject = m_player->getLastHitObject();
	if (hitObject)
	{
		for (Enemy* enemy : m_enemies)
		{
			GameObject* hitGameObject = static_cast<GameObject*>(hitObject->getUserPointer());
			if (hitGameObject == enemy)
			{
				bool wasDead = enemy->isDead();
				enemy->takeDamage(25);
				if (!wasDead && enemy->isDead())
					m_killCount++;
				break;
			}
		}
		for (FogEnemy* fogEnemy : m_fogEnemies)
		{
			GameObject* hitGameObject = static_cast<GameObject*>(hitObject->getUserPointer());
			if (hitGameObject == fogEnemy)
			{
				bool wasDead = fogEnemy->isDead();
				fogEnemy->takeDamage(25);
				if (!wasDead && fogEnemy->isDead())
					m_killCount++;
				break;
			}
		}
	}

	// Check enemy attack overlap with player
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

	// Remove dead enemies
	for (auto it = m_enemies.begin(); it != m_enemies.end(); )
	{
		if ((*it)->shouldRemove())
		{
			delete *it;
			it = m_enemies.erase(it);
		}
		else
			++it;
	}
	for (auto it = m_fogEnemies.begin(); it != m_fogEnemies.end(); )
	{
		if ((*it)->shouldRemove())
		{
			delete *it;
			it = m_fogEnemies.erase(it);
		}
		else
			++it;
	}

	// Pickup spawn timer — randomly respawn a hidden pickup
	m_pickupSpawnTimer -= deltaTime;
	if (m_pickupSpawnTimer <= 0.0f)
	{
		std::vector<int> hiddenIndices;
		for (int i = 0; i < (int)m_pickups.size(); i++)
		{
			if (m_pickups[i]->isCollected())
				hiddenIndices.push_back(i);
		}
		if (!hiddenIndices.empty())
		{
			int pick = hiddenIndices[rand() % hiddenIndices.size()];
			m_pickups[pick]->respawn();
		}
		m_pickupSpawnTimer = PICKUP_SPAWN_MIN + static_cast<f32>(rand()) / RAND_MAX * (PICKUP_SPAWN_MAX - PICKUP_SPAWN_MIN);
	}

	// Check pickup overlap
	for (Pickup* p : m_pickups)
	{
		p->update(deltaTime);
		if (!p->isCollected()
			&& p->getTrigger() && m_player->getBody()
			&& m_physics->isGhostOverlapping(p->getTrigger(), m_player->getBody()))
		{
			m_player->addAmmo(PICKUP_AMMO_AMOUNT);
			p->collect();
		}
	}

	updateCamera();
	updateHUD();

	if (crosshair)
	{
		bool showCrosshair = m_input.isRightMouseDown();
		crosshair->setVisible(showCrosshair);
	}
}

void Game::updateGameOver(f32 deltaTime)
{
	if (m_input.consumeKeyPress(KEY_ESCAPE) || (m_input.consumeLeftClick() && isClickInRect(m_endScreenExitBtnRect)))
	{
		m_device->closeDevice();
	}
}

void Game::updateWin(f32 deltaTime)
{
	if (m_input.consumeKeyPress(KEY_ESCAPE) || (m_input.consumeLeftClick() && isClickInRect(m_endScreenExitBtnRect)))
	{
		m_device->closeDevice();
	}
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

	// Timer display (MM:SS)
	s32 totalSeconds = (s32)ceilf(m_gameTimer);
	if (totalSeconds < 0) totalSeconds = 0;
	s32 minutes = totalSeconds / 60;
	s32 seconds = totalSeconds % 60;
	wchar_t timerStr[16];
	swprintf(timerStr, 16, L"%d:%02d", minutes, seconds);
	m_timerText->setText(timerStr);

	// Wave display
	wchar_t waveStr[16];
	swprintf(waveStr, 16, L"Wave %d", m_currentWave);
	m_waveText->setText(waveStr);

	// Kill count
	wchar_t killStr[32];
	swprintf(killStr, 32, L"Kills: %d", m_killCount);
	m_killText->setText(killStr);
}

bool Game::isClickInRect(const rect<s32>& r) const
{
	position2d<s32> cursor = m_device->getCursorControl()->getPosition();
	return r.isPointInside(cursor);
}

void Game::setHUDVisible(bool visible)
{
	if (m_ammoText) m_ammoText->setVisible(visible);
	if (m_healthText) m_healthText->setVisible(visible);
	if (m_timerText) m_timerText->setVisible(visible);
	if (m_waveText) m_waveText->setVisible(visible);
	if (m_killText) m_killText->setVisible(visible);
	if (crosshair) crosshair->setVisible(false); // crosshair managed separately
}

void Game::updateMenu()
{
	// Press T to enter testing mode
	if (m_input.consumeKeyPress(KEY_KEY_T))
	{
		m_state = GameState::TESTING;
		m_device->getCursorControl()->setVisible(false);
		m_device->getCursorControl()->setPosition(m_centerX, m_centerY);
		m_lastTime = m_device->getTimer()->getTime();
		setHUDVisible(true);
		return;
	}

	if (m_input.consumeLeftClick())
	{
		if (isClickInRect(m_playBtnRect))
		{
			// Show loading screen for 2 seconds
			{
				dimension2d<u32> ss = m_driver->getScreenSize();
				u32 startMs = m_device->getTimer()->getTime();
				while (m_device->run() && (m_device->getTimer()->getTime() - startMs) < 2000)
				{
					m_driver->beginScene(true, true, SColor(0, 0, 0, 0));
					if (m_menuBgTex)
					{
						dimension2d<u32> ts = m_menuBgTex->getOriginalSize();
						m_driver->draw2DImage(m_menuBgTex, rect<s32>(0, 0, ss.Width, ss.Height),
							rect<s32>(0, 0, ts.Width, ts.Height));
					}
					IGUIFont* font = m_gui->getSkin()->getFont();
					if (font)
					{
						font->draw(L"Loading...",
							rect<s32>(30, ss.Height - 60, 300, ss.Height - 20),
							SColor(255, 255, 255, 255), false, false);
					}
					m_driver->endScene();
				}
			}

			m_state = GameState::PLAYING;
			m_device->getCursorControl()->setVisible(false);
			m_device->getCursorControl()->setPosition(m_centerX, m_centerY);
			m_lastTime = m_device->getTimer()->getTime();
			setHUDVisible(true);
		}
		else if (isClickInRect(m_creditsBtnRect))
		{
			m_state = GameState::CREDITS;
		}
		else if (isClickInRect(m_exitBtnRect))
		{
			m_device->closeDevice();
		}
	}
}

void Game::updatePaused()
{
	if (m_input.consumeKeyPress(KEY_ESCAPE))
	{
		m_state = GameState::PLAYING;
		m_device->getCursorControl()->setVisible(false);
		m_device->getCursorControl()->setPosition(m_centerX, m_centerY);
		return;
	}

	if (m_input.consumeLeftClick())
	{
		if (isClickInRect(m_resumeBtnRect))
		{
			m_state = GameState::PLAYING;
			m_device->getCursorControl()->setVisible(false);
			m_device->getCursorControl()->setPosition(m_centerX, m_centerY);
		}
		else if (isClickInRect(m_pauseExitBtnRect))
		{
			m_device->closeDevice();
		}
	}
}

void Game::updateCredits()
{
	if (m_input.consumeKeyPress(KEY_ESCAPE) || m_input.consumeLeftClick())
	{
		m_state = GameState::MENU;
	}
}

void Game::drawMenu()
{
	dimension2d<u32> ss = m_driver->getScreenSize();

	// Background
	if (m_menuBgTex)
	{
		dimension2d<u32> texSize = m_menuBgTex->getOriginalSize();
		m_driver->draw2DImage(m_menuBgTex,
			rect<s32>(0, 0, ss.Width, ss.Height),
			rect<s32>(0, 0, texSize.Width, texSize.Height));
	}

	// Logo
	if (m_logoTex)
	{
		dimension2d<u32> ts = m_logoTex->getOriginalSize();
		m_driver->draw2DImage(m_logoTex, m_logoRect,
			rect<s32>(0, 0, ts.Width, ts.Height), 0, 0, true);
	}

	// Buttons
	if (m_playBtnTex)
	{
		dimension2d<u32> ts = m_playBtnTex->getOriginalSize();
		m_driver->draw2DImage(m_playBtnTex, m_playBtnRect,
			rect<s32>(0, 0, ts.Width, ts.Height), 0, 0, true);
	}
	if (m_creditsBtnTex)
	{
		dimension2d<u32> ts = m_creditsBtnTex->getOriginalSize();
		m_driver->draw2DImage(m_creditsBtnTex, m_creditsBtnRect,
			rect<s32>(0, 0, ts.Width, ts.Height), 0, 0, true);
	}
	if (m_exitBtnTex)
	{
		dimension2d<u32> ts = m_exitBtnTex->getOriginalSize();
		m_driver->draw2DImage(m_exitBtnTex, m_exitBtnRect,
			rect<s32>(0, 0, ts.Width, ts.Height), 0, 0, true);
	}
}

void Game::drawPause()
{
	dimension2d<u32> ss = m_driver->getScreenSize();

	// Dark semi-transparent overlay
	m_driver->draw2DRectangle(SColor(150, 0, 0, 0),
		rect<s32>(0, 0, ss.Width, ss.Height));

	// Resume button
	if (m_resumeBtnTex)
	{
		dimension2d<u32> ts = m_resumeBtnTex->getOriginalSize();
		m_driver->draw2DImage(m_resumeBtnTex, m_resumeBtnRect,
			rect<s32>(0, 0, ts.Width, ts.Height), 0, 0, true);
	}

	// Exit button
	if (m_exitBtnTex)
	{
		dimension2d<u32> ts = m_exitBtnTex->getOriginalSize();
		m_driver->draw2DImage(m_exitBtnTex, m_pauseExitBtnRect,
			rect<s32>(0, 0, ts.Width, ts.Height), 0, 0, true);
	}
}

void Game::drawCredits()
{
	dimension2d<u32> ss = m_driver->getScreenSize();

	// Background
	if (m_menuBgTex)
	{
		dimension2d<u32> texSize = m_menuBgTex->getOriginalSize();
		m_driver->draw2DImage(m_menuBgTex,
			rect<s32>(0, 0, ss.Width, ss.Height),
			rect<s32>(0, 0, texSize.Width, texSize.Height));
	}

	// Credits text
	IGUIFont* font = m_gui->getSkin()->getFont();
	if (font)
	{
		font->draw(L"CREDITS", rect<s32>(0, ss.Height/4, ss.Width, ss.Height/4 + 40),
			SColor(255, 255, 255, 255), true, true);
		font->draw(L"Survive - Arena Shooter", rect<s32>(0, ss.Height/4 + 60, ss.Width, ss.Height/4 + 100),
			SColor(255, 255, 200, 0), true, true);
		font->draw(L"Click or press ESC to go back", rect<s32>(0, ss.Height - 80, ss.Width, ss.Height - 40),
			SColor(255, 180, 180, 180), true, true);
	}
}

void Game::drawGameOver()
{
	dimension2d<u32> ss = m_driver->getScreenSize();

	// Dark overlay
	m_driver->draw2DRectangle(SColor(180, 0, 0, 0),
		rect<s32>(0, 0, ss.Width, ss.Height));

	IGUIFont* font = m_gui->getSkin()->getFont();
	if (font)
	{
		font->draw(L"GAME OVER", rect<s32>(0, ss.Height/2 - 80, ss.Width, ss.Height/2 - 40),
			SColor(255, 255, 50, 50), true, true);

		wchar_t killStr[64];
		swprintf(killStr, 64, L"Kills: %d", m_killCount);
		font->draw(killStr, rect<s32>(0, ss.Height/2 - 20, ss.Width, ss.Height/2 + 20),
			SColor(255, 255, 255, 255), true, true);
	}

	if (m_exitBtnTex)
	{
		dimension2d<u32> ts = m_exitBtnTex->getOriginalSize();
		m_driver->draw2DImage(m_exitBtnTex, m_endScreenExitBtnRect,
			rect<s32>(0, 0, ts.Width, ts.Height), 0, 0, true);
	}
}

void Game::drawWin()
{
	dimension2d<u32> ss = m_driver->getScreenSize();

	// Dark overlay
	m_driver->draw2DRectangle(SColor(180, 0, 0, 0),
		rect<s32>(0, 0, ss.Width, ss.Height));

	IGUIFont* font = m_gui->getSkin()->getFont();
	if (font)
	{
		font->draw(L"YOU SURVIVED!", rect<s32>(0, ss.Height/2 - 80, ss.Width, ss.Height/2 - 40),
			SColor(255, 50, 255, 50), true, true);

		wchar_t killStr[64];
		swprintf(killStr, 64, L"Kills: %d", m_killCount);
		font->draw(killStr, rect<s32>(0, ss.Height/2 - 20, ss.Width, ss.Height/2 + 20),
			SColor(255, 255, 255, 255), true, true);
	}

	if (m_exitBtnTex)
	{
		dimension2d<u32> ts = m_exitBtnTex->getOriginalSize();
		m_driver->draw2DImage(m_exitBtnTex, m_endScreenExitBtnRect,
			rect<s32>(0, 0, ts.Width, ts.Height), 0, 0, true);
	}
}
