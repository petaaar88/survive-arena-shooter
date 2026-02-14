#pragma once
#include <irrlicht.h>
#include <irrklang.h>
#include <vector>
#include "InputHandler.h"
#include "Physics.h"
#include "Player.h"
#include "Enemy.h"
#include "Pickup.h"
#include "DebugDrawer.h"

using namespace irr;
using namespace core;
using namespace scene;
using namespace video;
using namespace gui;

enum class GameState { MENU, PLAYING, PAUSED, CREDITS, GAMEOVER, WIN };

class Game
{
public:
	Game();
	~Game();
	void run();

private:
	void init();
	void setupScene();
	void setupGates(IMeshSceneNode* map);
	void setupHUD();

	void updateMenu();
	void updatePlaying(f32 deltaTime);
	void updatePaused();
	void updateCredits();
	void updateGameOver(f32 deltaTime);
	void updateWin(f32 deltaTime);
	void updateCamera();
	void updateHUD();
	void spawnEnemyAtGate(int gateIndex);
	void setHUDVisible(bool visible);

	void drawMenu();
	void drawPause();
	void drawCredits();
	void drawGameOver();
	void drawWin();

	bool isClickInRect(const rect<s32>& r) const;

	// Irrlicht core
	IrrlichtDevice*    m_device;
	IVideoDriver*      m_driver;
	ISceneManager*     m_smgr;
	IGUIEnvironment*   m_gui;
	InputHandler       m_input;

	// Physics
	Physics*           m_physics;
	DebugDrawer*       m_debugDrawer;
	btRigidBody*       m_groundBody;
	bool               m_showDebug;

	// Game objects
	Player*            m_player;
	std::vector<Enemy*> m_enemies;
	std::vector<Pickup*> m_pickups;
	f32                m_pickupSpawnTimer;
	ICameraSceneNode*  m_camera;
	ISceneNode*        m_ground;

	// HUD
	IGUIStaticText*    m_ammoText;
	IGUIStaticText*    m_healthText;
	IGUIStaticText*    m_timerText;
	IGUIStaticText*    m_waveText;
	IGUIStaticText*    m_killText;
	s32                m_killCount;

	// State
	GameState          m_state;
	f32                m_cameraYaw;
	u32                m_lastTime;
	s32                m_centerX;
	s32                m_centerY;

	IGUIImage* crosshair = nullptr;

	// Wave system
	f32 m_gameTimer;
	f32 m_spawnTimer;
	s32 m_currentWave;

	// Gate spawn positions (for enemy spawning)
	std::vector<vector3df> m_gatePositions;

	// Enemy audio
	irrklang::ISoundEngine* m_soundEngine;
	irrklang::ISound*       m_chasingSound;

	// Menu textures
	ITexture* m_menuBgTex;
	ITexture* m_playBtnTex;
	ITexture* m_creditsBtnTex;
	ITexture* m_exitBtnTex;
	ITexture* m_resumeBtnTex;

	// Button rects (screen space)
	rect<s32> m_playBtnRect;
	rect<s32> m_creditsBtnRect;
	rect<s32> m_exitBtnRect;
	rect<s32> m_resumeBtnRect;
	rect<s32> m_pauseExitBtnRect;
	rect<s32> m_endScreenExitBtnRect;
};
