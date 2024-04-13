#pragma once


struct KeyPacket
{
	static constexpr int _NO_KEY{ -1 };

	KeyPacket(const int bound_keycode, const int alt_keycode = _NO_KEY)
		: _bound_keycode{ bound_keycode }
		, _alt_keycode{ alt_keycode == _NO_KEY ? bound_keycode : alt_keycode }
	{	}

	
	int _bound_keycode{ 0 };
	int _alt_keycode{ 0 };


	void reset()
	{
		_was_pressed = false;
		_was_released = false;
	}


	void bigReset()
	{
		_was_pressed = false;
		_was_released = false;
		
		_is_pressed = false;
		_is_just_pressed = false;
		_is_just_released = false;
	}


	bool _was_pressed{ false };
	bool _was_released{ false };

	bool _is_pressed{ false };
	
	bool _is_just_pressed{ false };
	bool _is_just_released{ false };
};


// These actions are tied to the keyPackets of corresponding index.
// keyPackets_[0] will be the first action, etc.
enum Action
{
	Left = 0,
	Right,
	Up,
	Down,

	Telescope,
	
	Shift_Worlds,

	Unstick,

	Board,

	Jump,

	Quit,

	// Used to iterate over enum values.
	Count,

	// Special action, usually set as default value in ctors.
	NoAction,

	// Actions with alternate names listed below. 
	// Assign them the same value as their primary name.
	Forward = 2,
	Backward = 3,
	ZoomIn = Up,
	ZoomOut = Down,
};


class KeyboardManager
{
public:
	KeyboardManager() = default;

	void update();

	void keyPressed(const int keycode);

	void keyReleased(const int keycode);
	
	const bool isPressed(const Action action) const;

	const bool isJustPressed(const Action action) const;

	const bool isJustReleased(const Action action) const;

	void setKeyBinding(const Action action, const int new_keycode);

	void releaseAll();

	void mouseMovedTo(const int2 position);

	int2 getMousePosition() const;


private:
	const int getPacketIndex(const int keycode) const;

	// Initial keybinding should match the action of the keyPacket.
	// keyPackets_[0] is the first action and should have a corresponding keybinding.
	// Provide a 2nd keybinding if you want the action to have an alternate key binding.
	// Actions with alternate names share the same packet. Do not make another packet for them. 
	// Action with alterntae names do not require a 2nd keybinding. That is a separate choice.
	KeyPacket _key_packets[Action::Count]
	{
		// Movement keys (for player).
		GLFW_KEY_A,
		GLFW_KEY_D,
		GLFW_KEY_W,
		GLFW_KEY_S,

		// Telescope.
		GLFW_KEY_Q,

		// Rotate world.
		GLFW_KEY_R,

		// Unstick.
		GLFW_KEY_K,

		// Board
		GLFW_KEY_E,

		// Jump
		GLFW_KEY_SPACE,

		// Exit
		GLFW_KEY_ESCAPE,
	};

	// Yes, temporarily housing the mouse position in the keyboard manager for now.
	int2 _mouse_position{ -1, -1 };
};