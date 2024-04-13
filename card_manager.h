#pragma once


class CardManager
{
public:
	CardManager(const char* icon_filename);

	void setText(const std::string& card_text);

	bool update(const float delta_time);

	void copyTrimmedCardTo(Surface* screen);


	int _padding{ 2 };
	
	Surface _card;
	int2 _card_extents{ 1, 1 };

	Surface _icon;
	int _icon_size{ 0 };

	static constexpr float _MAX_DISPLAY_TIME{ 4.0f };
	float _elapsed_display_time{ 0.0f };

	int _significant_card_length{ 0 };
};