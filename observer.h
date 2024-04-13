#pragma once


class Observer
{
public:
	virtual ~Observer() {}

	virtual void onNotify(uint event_type, uint event_id) = 0;
};