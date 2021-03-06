#ifndef CONSOLE_HPP
#define CONSOLE_HPP

#include <iostream>
#include <vector>
#include <lua.hpp>
#include "TextBox.hpp"

#define MAX_HISTORY 20

class Console : public GuiElement{
	private:
		sf::RectangleShape bg;

		sf::Vector2i size;
		sf::Vector2f pos;
		sf::Color bgColour;
		sf::Clock inputTimer;
		lua_State *l;

		int currentPos;
		std::vector<std::string> history;
	
	public:
		ScrollText out;
		TextBox in;

		Console(lua_State *l,sf::Vector2f pos, sf::Color bg, sf::Color fg);

		void update(InputManager *im);
		void draw(sf::RenderWindow *screen);

};

#endif
