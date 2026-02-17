#pragma once
#include <irrlicht.h>

using namespace irr;

class InputHandler : public IEventReceiver
{
public:
	InputHandler() : m_leftMouseDown(false), m_leftMousePressed(false), m_rightMouseDown(false), m_rightMousePressed(false), m_mouseX(0), m_mouseY(0)
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
			m_mouseX = event.MouseInput.X;
			m_mouseY = event.MouseInput.Y;
			if (event.MouseInput.Event == EMIE_LMOUSE_PRESSED_DOWN)
			{
				m_leftMouseDown = true;
				m_leftMousePressed = true;
			}
			else if (event.MouseInput.Event == EMIE_LMOUSE_LEFT_UP)
				m_leftMouseDown = false;
			else if (event.MouseInput.Event == EMIE_RMOUSE_PRESSED_DOWN)
			{
				m_rightMouseDown = true;
				m_rightMousePressed = true;
			}
			else if (event.MouseInput.Event == EMIE_RMOUSE_LEFT_UP)
				m_rightMouseDown = false;
		}
		return false;
	}

	bool isKeyDown(EKEY_CODE key) const { return m_keys[key]; }
	bool isLeftMouseDown() const { return m_leftMouseDown; }
	bool isRightMouseDown() const { return m_rightMouseDown; }
	s32 getMouseX() const { return m_mouseX; }
	s32 getMouseY() const { return m_mouseY; }

	// Returns true once per key press (consumes the state)
	bool consumeKeyPress(EKEY_CODE key)
	{
		if (m_keys[key])
		{
			m_keys[key] = false;
			return true;
		}
		return false;
	}

	bool consumeLeftClick()
	{
		if (m_leftMousePressed)
		{
			m_leftMousePressed = false;
			return true;
		}
		return false;
	}

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
	bool m_leftMousePressed;
	bool m_rightMouseDown;
	bool m_rightMousePressed;
	s32 m_mouseX;
	s32 m_mouseY;
};
