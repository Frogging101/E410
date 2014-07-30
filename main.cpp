#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <iostream>
#include <sstream>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <lua.hpp>
#include "globals.hpp"
#include "Graphics/GraphicsUtils.hpp"
#include "Graphics/model.hpp"
#include "InputManager.hpp"
#include "ResourceManager.hpp"
#include "Graphics/RenderManager.hpp"
#include "Graphics/Sprite.hpp"
#include "GUI/Console.hpp"
#include "Lua/luabinding.hpp"
#include "Lua/GameObjectBinding.hpp"
#include "Networking/server.hpp"
#include "Lua/SettingsBinding.hpp"
using namespace std;

Console *global_con;
InputManager *im;
ResourceManager resman;
RenderManager rendman;
GuiManager *gui;
SoundManager *soundman;
ENetPeer *serverPeer;
ENetHost *client;
SDL_Window *screen = NULL;
bool running;
vector<string> packetList;
ShaderProgram *debugprg;
GLuint debugvbo;

int width, height;

const char *serverDelete = \
	"function __serverDelete(id)\n"
	"for k,v in pairs(serverObjects) do\n"
	"if v:getID() == id then\n"
	"v:remove()\n"
	"table.remove(serverObjects, k)\n"
	"return\n"
	"end\n"
	"end\n"
	"end\n"
	"function __addServerObject(obj)\n"
	"table.insert(serverObjects, obj)\n"
	"return #serverObjects\n"
	"end\n"
	"function __getServerObject(id)\n"
	"for k,v in pairs(serverObjects) do\n"
	"if v:getID() == id then\n"
	"return v\n"
	"end\n"
	"end\n"
	"return nil\n"
	"end\n";


int stringToInt(string input){
	int out;
	stringstream ss;
	ss << input;
	ss >> out;
	return out;	
}
float stringToFloat(string input){
	float out;
	stringstream ss;
	ss << input;
	ss >> out;
	return out;	
}
string intToString(int input){
	stringstream out;
	out << input;
	return out.str();
}
string floatToString(float input){
	stringstream out;
	out << input;
	return out.str();
}

void errMsg(string msg){
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error!",
			msg.c_str(), NULL); //<- change this to screen when we have that
}

void checkSDLError(){
	const char *error = SDL_GetError();
	if(*error != '\0'){
		errMsg(error);
		SDL_ClearError();
	}
}

int main(int argc, char *argv[]){
	srand(time(NULL));
	if(SDL_Init(SDL_INIT_EVERYTHING) == -1){
		errMsg("Could not intialize SDL");
		return EXIT_FAILURE;
	}
	checkSDLError();
	int flags = IMG_INIT_JPG | IMG_INIT_JPG;
	int initted = IMG_Init(flags);
	if((initted&flags) != flags){
		errMsg("Could not load JPG and PNG loader");
		return EXIT_FAILURE;
	}
	checkSDLError();
	if(TTF_Init() == -1){
		errMsg(TTF_GetError());
		return EXIT_FAILURE;
	}
	checkSDLError();
	//Set up audio device thing
	int audio_rate = 22050;
	uint16_t audio_format = AUDIO_S16;
	int audio_channels = 2;
	int audio_buffers = 4096;
	if(Mix_OpenAudio(audio_rate, audio_format, audio_channels, audio_buffers)){
		errMsg("Could not open audio device!");
		return EXIT_FAILURE;
	}
	SDL_ClearError();
	Mix_AllocateChannels(16);
	//Set Default settings
	width = 800;
	height = 600;
	settings.vsync = true;
	settings.fullscreen = false;
	settings.maxFPS = 0;
	settings.AA = 1;
	settings.width = width;
	settings.height = height;
	//Create lua vm and generate bindings
	lua_State *l = luaL_newstate();
	luaL_openlibs(l);
	bindFunctions(l);

	//Load server delete function
	if(luaL_dostring(l,serverDelete)){
		cerr << lua_tostring(l, -1) << endl;
		return EXIT_FAILURE;
	}

	//Load settings from lua
	if(luaL_dofile(l,"./data/scripts/settings.lua")){
		cerr << lua_tostring(l, -1) << endl;
		return EXIT_FAILURE;
	}

	//Load enet for networking
	if(enet_initialize() != 0){
		cerr << "Could not initalize ENet" << endl;
		return EXIT_FAILURE;
	}
	atexit(enet_deinitialize);
	ENetEvent enetEvent;

	//Create SDL window with opengl context
	SDL_GLContext context;
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
	screen = SDL_CreateWindow("E410|Dev", SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,
			settings.width, settings.height,
			SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL);
	checkSDLError();
	context = SDL_GL_CreateContext(screen);
	checkSDLError();
	if(settings.vsync)
		SDL_GL_SetSwapInterval(1);
	else
		SDL_GL_SetSwapInterval(0);

	InputManager ime;
	im = &ime;

	Console con(l,glm::vec2(0,0),
			glm::vec4(0.2,0.2,0.2,1), glm::vec4(1,1,1,1));

	//Create sound manager
	SoundManager m;
	soundman = &m;

	glewExperimental = GL_TRUE;
	GLenum glewStatus = glewInit();
	if(glewStatus != GLEW_OK){
		errMsg((char*)glewGetErrorString(glewStatus));
		return EXIT_FAILURE;
	}

	if(GLEW_VERSION_3_3 == false){
		errMsg("OpenGL 3.3 not supported!");
		return EXIT_FAILURE;
	}
	/*if(!GLEW_ARB_uniform_buffer_object){
		errMsg("UBOs not supported!");
		return EXIT_FAILURE;
	}
	if(!GLEW_ARB_texture_cube_map_array){
		errMsg("Cubemap arrays not supported!");
		return EXIT_FAILURE;
	}*/

	bool programsGood = true;

	ShaderProgram skyprg("./data/shaders/skyvertex.glsl",
			"./data/shaders/skyfragment.glsl");
	if(!skyprg.good)
		cerr << "Bad skybox shader program. No skybox will be rendered." << endl;

	debugprg = new ShaderProgram("./data/shaders/debuglinesv.glsl","./data/shaders/debuglinesf.glsl");
	glGenBuffers(1,&debugvbo);
	if(!debugprg->good)
		cerr << "Bad physics debug shader program. No collision debug info will be rendered." << endl;

	//Load the skybox
	rendman.skybox.setModel(resman.loadModel("skybox.iqm"));

	ShaderProgram prg("./data/shaders/vertex.glsl",
			"./data/shaders/fragment.glsl");
	if(!prg.good){
		programsGood = false;
		cerr << "Bad main shader program. Execution cannot continue." << endl;
	}

	ShaderProgram depthPrg("./data/shaders/depthv.glsl",
			"./data/shaders/depthf.glsl");
	if(!depthPrg.good){
		programsGood = false;
		cerr << "Bad depth shader program. Execution cannot continue." << endl;
	}

	ShaderProgram deferredPrg("./data/shaders/deferredv.glsl","./data/shaders/deferredf.glsl");
	if(!deferredPrg.good){
		programsGood = false;
		cerr << "Bad deferred shader program. Execution cannot continue." << endl;
	}

	ShaderProgram sprg("./data/shaders/spritev.glsl","./data/shaders/spritef.glsl");
	if(!sprg.good){
		programsGood = false;
		cerr << "Bad deferred shader program. Execution cannot continue." << endl;
	}
	spriteprg = &sprg;

	ShaderProgram gprg("./data/shaders/guivertex.glsl","./data/shaders/guifragment.glsl");
	if(!gprg.good){
		programsGood = false;
		cerr << "Bad gui shader program. Execution cannot continue." << endl;
	}

	if(!programsGood){
		errMsg("Shaders failed to compile\nCheck console for more details");
		return EXIT_FAILURE;
	}

	//Set some opengl stuff
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glEnable(GL_TEXTURE_2D);
	glClearColor(0.0,0.0,0.0,1.0);

	//Create GUI manager
	GuiManager guie(im, &gprg);
	gui = &guie;
	global_con = &con;
	guie.add(&con);
	
	//Load main lua file and then call init function
	lua_pushnumber(l, width);
	lua_setglobal(l, "width");
	lua_pushnumber(l, height);
	lua_setglobal(l, "height");
	lua_newtable(l);
	lua_setglobal(l, "serverObjects");
	int status = luaL_dofile(l, "./data/scripts/main.lua");
	if(status){
		cerr << lua_tostring(l,-1) << endl; 
		return EXIT_FAILURE;
	}

	glGenFramebuffers(1,&rendman.framebuffer);
	glGenRenderbuffers(1,&rendman.renderbuffer);
	glGenBuffers(1, &rendman.ubo);


	glBindBufferARB(GL_UNIFORM_BUFFER,rendman.ubo);
	glBufferDataARB(GL_UNIFORM_BUFFER,sizeof(glm::mat4)*MAX_LIGHTS+sizeof(glm::vec4)*(1+MAX_LIGHTS*6),NULL,GL_DYNAMIC_DRAW);
	glBindBufferBase(GL_UNIFORM_BUFFER,0,rendman.ubo);
	glBindBufferARB(GL_UNIFORM_BUFFER,0);

	glGenTextures(1,&rendman.normalTex);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D,rendman.normalTex);
	glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glGenTextures(1,&rendman.depthCubemaps);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY_ARB, rendman.depthCubemaps);
	glTexImage3D(GL_TEXTURE_CUBE_MAP_ARRAY_ARB,0,GL_DEPTH_COMPONENT16,512,512,MAX_LIGHTS*6,0,GL_DEPTH_COMPONENT,GL_FLOAT,NULL);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY_ARB, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY_ARB, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY_ARB, GL_TEXTURE_COMPARE_FUNC, GL_GEQUAL);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY_ARB, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_R_TO_TEXTURE);

	glBindRenderbuffer(GL_RENDERBUFFER,rendman.renderbuffer);
	glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,width,height);
	glBindRenderbuffer(GL_RENDERBUFFER,0);

	glUseProgram(prg.getID());
	glUniform1i(prg.getUniform("shadowCubes"), 1);
	glUniform1i(prg.getUniform("normalTex"), 2);
	glUniformBlockBinding(prg.getID(),glGetUniformBlockIndex(prg.getID(), "Light"),0);

	//Create projection matrix for main render
	glm::mat4 projection = glm::perspective(45.0f, 1.0f*width/height, 0.1f, 1000.0f);

	glUseProgram(skyprg.getID());
	glUniformMatrix4fv(skyprg.getUniform("projection"),1,GL_FALSE,glm::value_ptr(projection));
	glUseProgram(debugprg->getID());
	glUniformMatrix4fv(debugprg->getUniform("projection"),1,GL_FALSE,glm::value_ptr(projection));

	lua_getglobal(l, "init");
	status = lua_pcall(l,0,0,0);
	if(status){
		cout << lua_tostring(l, -1) << endl;
		global_con->out.println(lua_tostring(l, -1));
		return EXIT_FAILURE;
	}

	//Set stuff for main game loop
	Timer dtTimer;
	dtTimer.reset();
	float dt;

	int majv;
	int minv;
	glGetIntegerv(GL_MAJOR_VERSION, &majv);
	glGetIntegerv(GL_MINOR_VERSION, &minv);
	cout << "GL Version: "<< majv << "." << minv << endl;

	char *pstr = new char[65536];
	SDL_Event e;
	running = true;
	while(running){
		while(SDL_PollEvent(&e)){
			if(e.type == SDL_QUIT){
				running = false;
			}
			if(e.type == SDL_WINDOWEVENT){
				if(e.window.event == SDL_WINDOWEVENT_RESIZED){
					width = e.window.data1;
					height = e.window.data2;
					glViewport(0,0,width,height);


					lua_pushnumber(l, width);
					lua_setglobal(l, "width");
					lua_pushnumber(l, height);
					lua_setglobal(l, "height");
				} else if(e.window.event == SDL_WINDOWEVENT_FOCUS_GAINED){
					im->setFocus(true);
				} else if(e.window.event == SDL_WINDOWEVENT_FOCUS_LOST){
					im->setFocus(false);
				}
			}
			if(e.type == SDL_TEXTINPUT){
				im->addInput(string(e.text.text));
			}
			if(e.type == SDL_KEYDOWN){
				SDL_Keycode key = e.key.keysym.sym;
				//Toggle console is user hits f1
				if(key == SDLK_F1){
					im->getString();
					con.visible = !con.visible;
					con.updates = !con.updates;
					im->setGuiMousePos(glm::ivec2(width/2,height/2));
				} else if(im->isGuiLocked() == false){
					lua_getglobal(l, "onKeyDown");
					lua_pushnumber(l, key);
					if(lua_pcall(l,1,0,0)){
						cout << lua_tostring(l, -1) << endl;
						global_con->out.println(lua_tostring(l, -1));
					}

				}
			}
			if(e.type == SDL_KEYUP){
				SDL_Keycode key = e.key.keysym.sym;
				if(im->isGuiLocked() == false){
					lua_getglobal(l, "onKeyRelease");
					lua_pushnumber(l, key);
					if(lua_pcall(l,1,0,0)){
							cout << lua_tostring(l, -1) << endl;
							global_con->out.println(lua_tostring(l, -1));
					}
				}
			}
		}
		//Update sounds
		soundman->update();
		//Handle input packets and send buffered packets
		if(client != NULL){
			while(enet_host_service(client, &enetEvent, 0) >0){
				if(enetEvent.type == ENET_EVENT_TYPE_RECEIVE){
					//handle packets here
					memcpy(pstr,enetEvent.packet->data,enetEvent.packet->dataLength);
					pstr[enetEvent.packet->dataLength] = 0;
					string input = pstr;
					vector<string> pdata = breakString(input);
					if(pdata[0] == "create"){
						//Push the object to lua so
						//lua side code can control it if it wants to
						string modelName = pdata[1];
						Model *mod = resman.loadModel(modelName);
						if(mod == NULL){
							cerr << "Could not create model " << modelName
								<< endl;
							continue;
						}
						
						//lua_getglobal(l, "serverObjects");
						lua_getglobal(l, "__addServerObject");
						GameObject *out = new (lua_newuserdata(l, sizeof(GameObject))) GameObject;
						out->setModel(mod);
						out->magic = GOMAGIC;
						out->tag = pdata[2];
						out->id = stringToInt(pdata[3]);
						rendman.drawList.push_back(out);

						//Push gameobject onto global serverObjects table to keep track of it						
						luaL_getmetatable(l, "MetaGO");
						lua_setmetatable(l, -2);
						//lua_rawseti(l, -2, out->id);
						if(lua_pcall(l,1,1,0)){
							cout << lua_tostring(l, -1) << endl;
							global_con->out.println(lua_tostring(l, -1));
						}
						int index = l_toNumber(l,-1);

						//push gameobject to lua createObject so a game script can handle it
						lua_getglobal(l,"serverObjects");
						lua_getglobal(l, "createObject"); //push createObject
						lua_rawgeti(l, -2, index); // get serverObjects[id]
						if(lua_pcall(l,1,0,0)){ //call createObjects(serverObjects[id])
							cout << lua_tostring(l, -1) << endl;
							global_con->out.println(lua_tostring(l, -1));
						}

					}
					else if(pdata[0] == "move"){
						uint32_t id = stringToInt(pdata[1]);
						GameObject *obj = rendman.getId(id);
						if(obj == NULL){
							cerr << "Could not find object " << id << endl;
							continue;
						}
						obj->position.x = stringToFloat(pdata[2]);
						obj->position.y = stringToFloat(pdata[3]);
						obj->position.z = stringToFloat(pdata[4]);
					}
					else if(pdata[0] == "rotate"){
						uint32_t id = stringToInt(pdata[1]);
						GameObject *obj = rendman.getId(id);
						if(obj == NULL){
							cerr << "Could not find object " << id << endl;
							continue;
						}
						obj->rotation.x = stringToFloat(pdata[2]);
						obj->rotation.y = stringToFloat(pdata[3]);
						obj->rotation.z = stringToFloat(pdata[4]);
						obj->rot.setX(stringToFloat(pdata[5]));
						obj->rot.setY(stringToFloat(pdata[6]));
						obj->rot.setZ(stringToFloat(pdata[7]));
						obj->rot.setW(stringToFloat(pdata[8]));
						obj->updateLookat();
					}
					else if(pdata[0] == "scale"){
						uint32_t id = stringToInt(pdata[1]);
						GameObject *obj = rendman.getId(id);
						if(obj == NULL){
							cerr << "Could not find object " << id << endl;
							continue;
						}
						obj->scale.x = stringToFloat(pdata[2]);
						obj->scale.y = stringToFloat(pdata[3]);
						obj->scale.z = stringToFloat(pdata[4]);
					}else if(pdata[0] == "delete") {
						uint32_t id = stringToInt(pdata[1]);
						GameObject *obj = rendman.getId(id);
						if(obj != NULL){
							rendman.remove(obj);
							lua_getglobal(l,"__serverDelete");
							lua_pushnumber(l,obj->id);
							if(lua_pcall(l,1,0,0)){
								cout << lua_tostring(l, -1) << endl;
								global_con->out.println(lua_tostring(l, -1));
							}

						}
					}else if(pdata[0] == "visible") {
						uint32_t id = stringToInt(pdata[1]);
						GameObject *obj = rendman.getId(id);
						obj->visible = stringToInt(pdata[2]);
					}else {
						lua_getglobal(l,"onReceivePacket");
						l_pushStringVector(l,pdata);
						if(lua_pcall(l,1,0,0)){
							string error = lua_tostring(l,-1);
							cout << error << endl;
							global_con->out.print(error);
						}
					}

					enet_packet_destroy(enetEvent.packet);
				}else if(enetEvent.type == ENET_EVENT_TYPE_DISCONNECT){
					//handle server disconnect here
					serverPeer = NULL;
					enet_host_destroy(client);
					client = NULL;
					lua_getglobal(l, "onServerDisconnect");
					if(lua_pcall(l,0,0,0)){
						string error = lua_tostring(l,-1);
						cout << error << endl;
						global_con->out.print(error);
					}
					int index=1;
					lua_getglobal(l,"serverObjects");
					lua_pushvalue(l,-1);
					lua_pushnil(l);
					while(lua_next(l,-2) != 0){
						GameObject *obj=l_toGO(l,-1);
						rendman.remove(obj);
						lua_pushnil(l);
						lua_rawseti(l,-4,index);
						index++;
						lua_pop(l,1);
					}
					break;
				}
			}
			//send buffered packets
			for(int i=0;i<packetList.size();i++){
				const char *cstr = packetList[i].c_str();
				ENetPacket *packet = enet_packet_create(
						cstr,
						packetList[i].length(),
						ENET_PACKET_FLAG_RELIABLE);
				enet_peer_send(serverPeer, 0, packet);
				enet_host_flush(client);
			}
			packetList.clear();
		}
		
		//Check if gui is locked and show cursor
		if(im->isGuiLocked()){
			SDL_ShowCursor(true);
		} else {
			SDL_ShowCursor(false);
		}

		//call lua update
		lua_getglobal(l,"update");
		lua_pushnumber(l,dt);
		if(lua_pcall(l,1,0,0)){
			cout << lua_tostring(l, -1) << endl;
			global_con->out.println(lua_tostring(l, -1));
		}

		gui->update();
		for(int i=0;i<rendman.drawList.size();i++){
			if(rendman.drawList[i]->animate){
				rendman.drawList[i]->aTime += dt;
				rendman.drawList[i]->model->animate(rendman.drawList[i]->currentAnimation,
				rendman.drawList[i]->aTime,&rendman.drawList[i]->outframe);
			}
		}

		//Do all drawing here
		glEnable(GL_TEXTURE_CUBE_MAP_ARB);
		glUseProgram(depthPrg.getID());
		glBindFramebuffer(GL_FRAMEBUFFER,rendman.framebuffer);
		for(int i=0;i<rendman.lights.size();i++)
			rendman.renderDepth(&depthPrg,i);

		glUseProgram(deferredPrg.getID());
		glUniformMatrix4fv(deferredPrg.getUniform("projection"),1,GL_FALSE,glm::value_ptr(projection));
		rendman.renderDeferred(&deferredPrg);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glUseProgram(prg.getID());
		glUniformMatrix4fv(prg.getUniform("projection"),1,GL_FALSE,glm::value_ptr(projection));
		glUniformMatrix4fv(prg.getUniform("pointProj"),1,GL_FALSE,glm::value_ptr(PLight::pointProjection));
		rendman.render(&prg,&skyprg,dt);
		glDisable(GL_TEXTURE_CUBE_MAP_ARB);
		rendman.renderSprites(spriteprg,dt);


		//Do sfml drawing here
		glUseProgram(gprg.getID());
		glDisable(GL_DEPTH_TEST);
		glEnableVertexAttribArray(gprg.getAttribute("coord2d"));
		gui->draw(&gprg);
		glDisableVertexAttribArray(gprg.getAttribute("coord2d"));
		glEnable(GL_DEPTH_TEST);

		SDL_GL_SwapWindow(screen);
		dt = dtTimer.getElapsedTime();
		dtTimer.reset();
	}

	delete[] pstr;
	enet_host_destroy(client);
	serverRunning = false;
	SDL_WaitThread(serverThread, NULL);
	Mix_CloseAudio();
	SDL_Quit();
}
