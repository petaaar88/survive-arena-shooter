#pragma once
#include <irrlicht.h>
#include "InputHandler.h"
#include "Player.h"

using namespace irr;
using namespace core;
using namespace scene;
using namespace video;
using namespace gui;

enum class GameState { MENU, PLAYING, GAMEOVER, WIN };

class Game
{
public:
	Game();
	~Game();
	void run();

private:
	void init();
	void setupScene();
	void setupHUD();

	void updatePlaying(f32 deltaTime);
	void updateGameOver(f32 deltaTime);
	void updateCamera();
	void updateHUD();

	// Irrlicht core
	IrrlichtDevice*    m_device;
	IVideoDriver*      m_driver;
	ISceneManager*     m_smgr;
	IGUIEnvironment*   m_gui;
	InputHandler       m_input;

	// Game objects
	Player*            m_player;
	ICameraSceneNode*  m_camera;
	ISceneNode*        m_ground;

	// HUD
	IGUIStaticText*    m_ammoText;
	IGUIStaticText*    m_healthText;

	// State
	GameState          m_state;
	f32                m_cameraYaw;
	u32                m_lastTime;
	s32                m_centerX;
	s32                m_centerY;
};
