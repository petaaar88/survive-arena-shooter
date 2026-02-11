#pragma once
#include <irrlicht.h>

using namespace irr;

class InputHandler : public IEventReceiver
{
public:
	InputHandler() : m_leftMouseDown(false), m_rightMouseDown(false), m_rightMousePressed(false)
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
	bool m_rightMouseDown;
	bool m_rightMousePressed;
};
