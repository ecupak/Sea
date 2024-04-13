#include "precomp.h"
#include "keyboardManager.h"


// Update the state of the keypackets.
void KeyboardManager::update()
{
	for (KeyPacket& packet : _key_packets)
	{
		bool was_handled{ false };

		if (packet._was_pressed && !packet._was_released)
		{
			packet._is_just_pressed = !packet._is_pressed;
			packet._is_pressed = true;
			packet._is_just_released = false;
			was_handled = true;
		}
		
		if (packet._was_released && !packet._was_pressed)
		{
			packet._is_just_released = packet._is_pressed;
			packet._is_pressed = false;
			packet._is_just_pressed = false;
			was_handled = true;
		}

		if (!was_handled)
		{
			packet._is_just_pressed = false;
			packet._is_just_released = false;
		}

		packet.reset();
	}
}


// Setting keystate obtained by Game using GLFW keycodes.
void KeyboardManager::keyPressed(const int keycode)
{
	int index{ getPacketIndex(keycode) };

	if (index != -1)
	{
		_key_packets[index]._was_pressed = true;
	}
}


void KeyboardManager::keyReleased(const int keycode)
{
	int index{ getPacketIndex(keycode) };

	if (index != -1)
	{
		_key_packets[index]._was_released = true;
	}
}


// Returning keystate to objects using Action enum.
const bool KeyboardManager::isPressed(const Action action) const
{
	return _key_packets[action]._is_pressed;
}


const bool KeyboardManager::isJustPressed(const Action action) const
{
	return _key_packets[action]._is_just_pressed;
}


const bool KeyboardManager::isJustReleased(const Action action) const
{
	return _key_packets[action]._is_just_released;
}


void KeyboardManager::setKeyBinding(const Action, const int)
{
	
}


void KeyboardManager::releaseAll()
{
	for (int index{ 0 }; index < Action::Count; ++index)
	{
		_key_packets[index].bigReset();
	}
}


void KeyboardManager::mouseMovedTo(const int2 position)
{
	_mouse_position = position;
}


int2 KeyboardManager::getMousePosition() const
{
	return _mouse_position;
}


const int KeyboardManager::getPacketIndex(const int keycode) const
{
	for (int index{ 0 }; index < Action::Count; ++index)
	{
		if (keycode == _key_packets[index]._bound_keycode || keycode == _key_packets[index]._alt_keycode)
		{
			return index;
		}
	}

	return -1;
}