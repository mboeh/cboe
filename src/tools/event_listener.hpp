#ifndef EVENT_LISTENER_HPP
#define EVENT_LISTENER_HPP

#include <SFML/Graphics.hpp>

class iEventListener {
	
public:
	
	virtual bool handle_event(const sf::Event&) = 0;
	
	virtual ~iEventListener() {}
	
};

#endif
