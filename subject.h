#pragma once


class Subject
{
public:
	Subject() = default;
	~Subject();


	void addObserver(Observer* observer);


	void notify(uint event_type, uint event_id);


private:
	static constexpr int _MAX_OBSERVERS{ 5 };
	Observer* _observers[_MAX_OBSERVERS]{ nullptr };

	int _observer_current_index{ 0 };
};

