#pragma once


class AudioManager : public Observer
{
public:
	AudioManager(CardManager& audio_card);

	~AudioManager() override;

	void onNotify(uint event_type, uint event_id) override;


	CardManager& _audio_card;


private:
	void processMusicEvent(uint event_id);
	void processSFXEvent(uint event_id);

	void processEventDuringUrgentTrack(uint event_id);
	void processEventDuringNormalTrack(uint event_id);
	void swapUrgentOceanTracks();


	int _pending_event_id{ 0 };
	bool _is_urgent_track_playing{ false };
	float _music_volume{ 0.2f };
	float _sfx_ocean_volume{ 0.15f };

	// BGM
	struct SoundData
	{
		SoundData() = default;
		SoundData(const char* filepath, const char* audio_name)
			: _audio{ filepath, Audio::Sound::Type::Background }
			, _title{ audio_name }
		{	}

		Audio::Sound _audio;
		std::string _title{ "" };
	};

	SoundData _day_ocean_0{ "assets/audio/music/Angel_Share.mp3",				"Angel Share" };
	SoundData _day_ocean_1{ "assets/audio/music/Master_of_the_Feast.mp3",		"Master of the Feast" };
	SoundData _day_ocean_2{ "assets/audio/music/The_Forest_and_the_Trees.mp3",	"The Forest and the Trees" };
	SoundData* _day_ocean_list[3]{ &_day_ocean_0, &_day_ocean_1, &_day_ocean_2 };

	SoundData _day_island_0{ "assets/audio/music/Cattails.mp3",		"Cattails" };
	SoundData _day_island_1{ "assets/audio/music/Easy_Lemon.mp3",	"Easy Lemon" };
	SoundData _day_island_2{ "assets/audio/music/Morning.mp3",		"Morning" };
	SoundData* _day_island_list[3]{ &_day_island_0, &_day_island_1, &_day_island_2 };

	SoundData _night_ocean_0{ "assets/audio/music/Lasting_Hope.mp3",		"Lasting Hope" };
	SoundData _night_ocean_1{ "assets/audio/music/Equatorial_Complex.mp3",	"Equatorial Complex" };
	SoundData _night_ocean_2{ "assets/audio/music/Magic_Forest.mp3",		"Magic Forest" };
	SoundData* _night_ocean_list[3]{ &_night_ocean_0, &_night_ocean_1, &_night_ocean_2 };

	SoundData _night_island_0{ "assets/audio/music/Bleeping_Demo.mp3",	"Bleeping Demo" };
	SoundData _night_island_1{ "assets/audio/music/Galactic_Rap.mp3",	"Galactic Rap" };
	SoundData _night_island_2{ "assets/audio/music/Dark_Fog.mp3",		"Dark Fog" };
	SoundData* _night_island_list[3]{ &_night_island_0, &_night_island_1, &_night_island_2 };

	SoundData _day_urgent_0{ "assets/audio/music/Minima.mp3",			"Minima" };
	SoundData _day_urgent_1{ "assets/audio/music/Constance.mp3",		"Constance" };
	SoundData _day_urgent_2{ "assets/audio/music/Living_Voyage.mp3",	"Living Voyage" };
	SoundData* _day_urgent_list[3]{ &_day_urgent_0, &_day_urgent_1, &_day_urgent_2 };

	SoundData _night_urgent_0{ "assets/audio/music/Night_Minima.mp3",			"Minima" };
	SoundData _night_urgent_1{ "assets/audio/music/Night_Constance.mp3",		"Constance" };
	SoundData _night_urgent_2{ "assets/audio/music/Night_Living_Voyage.mp3",	"Living Voyage" };
	SoundData* _night_urgent_list[3]{ &_night_urgent_0, &_night_urgent_1, &_night_urgent_2 };

	SoundData _brightened_0{ "assets/audio/music/Bathed_in_the_Light.mp3", "Bathed in the Light" };

	SoundData* _current_track{ nullptr };
	SoundData* _muted_track{ nullptr };
	bool _current_urgent_track_is_day{ true };

	// SFX
	Audio::Sound _sfx_day_ocean{ "assets/audio/sfx/boat_waves.mp3",				Audio::Sound::Type::Stream };
	Audio::Sound _sfx_night_ocean{ "assets/audio/sfx/underwater_ambience.mp3",	Audio::Sound::Type::Stream };
	Audio::Sound _sfx_rumble{ "assets/audio/sfx/earth_rumble.mp3",				Audio::Sound::Type::Stream };

	Audio::Sound _sfx_upgrade{ "assets/audio/sfx/diamond_found_edit.mp3",		Audio::Sound::Type::Sound };
	Audio::Sound _sfx_decharge{ "assets/audio/sfx/error.mp3",					Audio::Sound::Type::Sound };
	Audio::Sound _sfx_brightened{ "assets/audio/sfx/chime_sound.mp3",			Audio::Sound::Type::Sound };
	Audio::Sound _sfx_shift{ "assets/audio/sfx/rising_choir.mp3",				Audio::Sound::Type::Sound };
	Audio::Sound _sfx_charge{ "assets/audio/sfx/fantasy_ui_button.mp3",			Audio::Sound::Type::Sound };
	Audio::Sound _sfx_death{ "assets/audio/sfx/bouncy_sound.mp3",				Audio::Sound::Type::Sound };
	Audio::Sound _sfx_jump{ "assets/audio/sfx/cartoon_jump.mp3",				Audio::Sound::Type::Sound };
};