#include "precomp.h"
#include "audio_manager.h"


AudioManager::AudioManager(CardManager& audio_card)
	: _audio_card{audio_card}
{	}


AudioManager::~AudioManager() {}


void AudioManager::onNotify(uint event_type, uint event_id)
{
	switch (event_type)
	{
		case EventType::MUSIC:
			processMusicEvent(event_id);
			break;
		case EventType::SFX:
			processSFXEvent(event_id);
			break;
		default:
			break;
	}
}


void AudioManager::processMusicEvent(uint event_id)
{
	// Different result based on if urgent track is playing.
	if (_is_urgent_track_playing)
	{
		processEventDuringUrgentTrack(event_id);
	}
	else
	{
		processEventDuringNormalTrack(event_id);
	}

	// Store the event. Once urgent track ends (orb loses charge)
	// the pending event will be considered.
	switch (event_id)
	{
		case MusicEvent::ON_SWITCH_TO_DAY:
			_pending_event_id = MusicEvent::ON_DAY_OCEAN;
			break;
		case MusicEvent::ON_SWITCH_TO_NIGHT:
			_pending_event_id = MusicEvent::ON_NIGHT_OCEAN;
			break;
		case MusicEvent::ON_ORB_HAS_CHARGE:
			break;
		default:
			_pending_event_id = event_id;
			break;
	}	
}


void AudioManager::processEventDuringUrgentTrack(uint event_id)
{
	// While urgent track is playing, flow to music is altered:
	//  - Ignores island events (stored as pending event).
	//  - On empty orb, switch to pending event.
	//  - On successful island brightening, play brightening audio.
	//  - On switch between worlds, swaps between lo-pass and normal version of track.
	switch (event_id)
	{
	case MusicEvent::ON_ORB_LOST_CHARGE:
	{
		_is_urgent_track_playing = false;

		_muted_track->_audio.stop();
		_muted_track = nullptr;
		
		processEventDuringNormalTrack(_pending_event_id);
		break;
	}
	case MusicEvent::ON_SWITCH_TO_NIGHT:
	{
		_sfx_day_ocean.stop();

		_sfx_night_ocean.seek(0);
		_sfx_night_ocean.setVolume(_sfx_ocean_volume);
		_sfx_night_ocean.play();
		
		swapUrgentOceanTracks();
		break;
	}
	case MusicEvent::ON_SWITCH_TO_DAY:
	{
		_sfx_night_ocean.stop();

		_sfx_day_ocean.seek(0);
		_sfx_day_ocean.setVolume(_sfx_ocean_volume);
		_sfx_day_ocean.play();
		
		swapUrgentOceanTracks();
		break;
	}	
	default:
		break;
	}
}


void AudioManager::processEventDuringNormalTrack(uint event_id)
{
	if (_current_track)
	{
		_current_track->_audio.stop();
	}

	int index{ static_cast<int>(RandomFloat() * 3.0f) };

	switch (event_id)
	{
	case MusicEvent::ON_DAY_ISLAND:
	{
		_current_track = _day_island_list[index];

		break;
	}
	case MusicEvent::ON_DAY_OCEAN:
	{
		_sfx_day_ocean.seek(0);
		_sfx_day_ocean.setLooping(true);
		_sfx_day_ocean.setVolume(_sfx_ocean_volume);
		_sfx_day_ocean.play();

		_current_track = _day_ocean_list[index];

		break;
	}
	case MusicEvent::ON_NIGHT_ISLAND:
	{
		_current_track = _night_island_list[index];

		break;
	}
	case MusicEvent::ON_NIGHT_OCEAN:
	{
		_sfx_night_ocean.seek(0);
		_sfx_night_ocean.setLooping(true);
		_sfx_night_ocean.setVolume(_sfx_ocean_volume);
		_sfx_night_ocean.play();

		_current_track = _night_ocean_list[index];

		break;
	}
	case MusicEvent::ON_ORB_HAS_CHARGE:
	{
		_is_urgent_track_playing = true;

		_current_track = _day_urgent_list[index];

		_muted_track = _night_urgent_list[index];
		_muted_track->_audio.setLooping(true);
		_muted_track->_audio.setVolume(0.0f);
		_muted_track->_audio.seek(0);
		_muted_track->_audio.play();

		break;
	}	
	case MusicEvent::ON_SWITCH_TO_NIGHT:
	{
		_sfx_day_ocean.stop();

		_sfx_night_ocean.seek(0);
		_sfx_night_ocean.setVolume(_sfx_ocean_volume);
		_sfx_night_ocean.play();

		_current_track = _night_ocean_list[index];

		break;
	}
	case MusicEvent::ON_SWITCH_TO_DAY:
	{
		_sfx_night_ocean.stop();

		_sfx_day_ocean.seek(0);
		_sfx_day_ocean.setVolume(_sfx_ocean_volume);
		_sfx_day_ocean.play();

		_current_track = _day_ocean_list[index];

		break;
	}
	default:
		break;
	}

	if (_current_track)
	{
		_current_track->_audio.setVolume(_music_volume);
		_current_track->_audio.setLooping(true);
		_current_track->_audio.seek(0);
		_current_track->_audio.play();
	
		_audio_card.setText(_current_track->_title);
	}
}


// Urgent song stays the same between shifts (low pass filter on night version).
// Night version also needs a small volume boost.
void AudioManager::swapUrgentOceanTracks()
{
	std::swap(_current_track, _muted_track);
	
	_current_urgent_track_is_day = !_current_urgent_track_is_day;

	_current_track->_audio.setVolume(_current_urgent_track_is_day ? _music_volume : _music_volume * 2.0f);
	_muted_track->_audio.setVolume(0.0f);
}


void AudioManager::processSFXEvent(uint event_id)
{
	switch (event_id)
	{
		case SFXEvent::ON_CHARGE_STONE_ACTIVATION:
		{
			_sfx_charge.seek(0);
			_sfx_charge.setVolume(0.3f);
			_sfx_charge.play();

			break;
		}		
		case SFXEvent::ON_ORB_REACHED_EMPTY:
		{
			_sfx_decharge.seek(0);
			_sfx_decharge.play();

			break;
		}
		case SFXEvent::ON_UPGRADE_STONE_ACTIVATION:
		{
			_sfx_upgrade.seek(0);
			_sfx_upgrade.play();

			break;
		}
		case SFXEvent::ON_ISLAND_MOVE:
		{
			_sfx_rumble.seek(0);
			_sfx_rumble.setLooping(true);
			_sfx_rumble.setVolume(_music_volume);			
			_sfx_rumble.play();

			break;
		}
		case SFXEvent::ON_ISLAND_STOP:	
		{	
			_sfx_rumble.stop();

			break;
		}
		case SFXEvent::ON_WATER_DEATH:
		{
			_sfx_death.seek(0);
			_sfx_death.setVolume(0.35f);
			_sfx_death.play();

			break;
		}
		case SFXEvent::ON_PIER_EMBARK:
		case SFXEvent::ON_PLAYER_JUMP:
		{
			_sfx_jump.seek(0);
			_sfx_jump.setVolume(0.3f);
			_sfx_jump.play();

			break;
		}
		case SFXEvent::ON_SHIFT_WORLDS:
		{
			_sfx_shift.seek(250);
			_sfx_shift.setVolume(0.05f);
			_sfx_shift.play();

			break;
		}
		case SFXEvent::ON_ISLAND_BRIGHTENED:
		{
			_is_urgent_track_playing = false;

			if (_muted_track)
			{
				_muted_track->_audio.stop();
				_muted_track = nullptr;
			}
						
			_sfx_brightened.seek(0);
			_sfx_brightened.setVolume(0.4f);
			_sfx_brightened.play();
			
			processEventDuringNormalTrack(_pending_event_id);
			break;
		}
		default:
			break;
	}
}