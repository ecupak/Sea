#include "precomp.h"
#include "card_manager.h"


CardManager::CardManager(const char* icon_filename)
	: _card{ SCRWIDTH, max(30, SCRHEIGHT / 16) }
	, _card_extents{ _card.width / 2, _card.height / 2 }
	, _icon{ icon_filename }
	, _icon_size{ _card.height - (_padding * 2)}
{	}


void CardManager::setText(const std::string& card_text)
{
	_card.Clear(0);
	
	// Copy icon to card, scaled to fit.
	_icon.ScaledCopyTo(&_card, _padding, _padding, _icon_size, _icon_size);

	// Draw vertically centered, next to icon. 1 padding before icon + 3 after (1 to balance before, 2 to give space before text).
	_card.Print(card_text.c_str(), _icon_size + (_padding * 4), _card_extents.y - 3, 0xFFFFFF);

	// Length of card trimmed to important parts. Add 3 padding to end.
	_significant_card_length = static_cast<int>(card_text.length() * 6u) + _icon_size + (_padding * (4 + 3));

	// Start display.
	_elapsed_display_time = 0.0f;
}


bool CardManager::update(const float delta_time)
{
	_elapsed_display_time += delta_time;

	return _elapsed_display_time < _MAX_DISPLAY_TIME;
}


void CardManager::copyTrimmedCardTo(Surface* screen)
{
	_card.TrimmedCopyTo(screen, 0, SCRHEIGHT - _card.height, _significant_card_length, _card.height);
}