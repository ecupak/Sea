#include "precomp.h"
#include "subject.h"


Subject::~Subject() {}


void Subject::addObserver(Observer* observer)
{
	if (_observer_current_index < _MAX_OBSERVERS)
	{
		_observers[_observer_current_index++] = observer;
	}
}


void Subject::notify(uint event_type, uint event_id)
{
	for (int index = 0; index < _observer_current_index; ++index)
	{
		_observers[index]->onNotify(event_type, event_id);
	}
}