#include "ResourceManager.hpp"
using namespace std;

string dataDir = "data/";
string modelDir = dataDir+"models/";
string fontDir = dataDir+"fonts/";
string imgDir = dataDir+"textures/";
string soundDir = dataDir+"audio/";

string defaultFont = "DejaVuSansMono.ttf";


/**
 * @param name Name of model, .iqm suffix currently required
 * @return Pointer to the model in the resource manager
 * Returns a pointer to a loaded model from the resource manager,
 * and loads it first if it is not already loaded.
 */
Model *ResourceManager::loadModel(string name){
	map<string,Model>::iterator it = models.find(name);
	//If model is not loaded, load it
	if(it == models.end()){
		Model model;
		if(loadIQM(modelDir+name,model) == false){
			return NULL;
		};
		models[name] = model;
		return &models[name];
	}else{
		return &it->second;
	}
}

Model *ResourceManager::loadNoGLModel(std::string name){
	map<string,Model>::iterator it = noGLModels.find(name);
	//If model is not loaded, load it
	if(it == noGLModels.end()){
		Model model;
		if(noGLLoadIQM(modelDir+name,model) == false){
			return NULL;
		};
		noGLModels[name] = model;
		return &noGLModels[name];
	}else{
		return &it->second;
	}
}

sf::Font *ResourceManager::loadFont(string name){
	map<string, sf::Font>::iterator it = fonts.find(name);

	if(it == fonts.end()){
		sf::Font f;
		if(f.loadFromFile(fontDir+name) == false){
			return NULL;
		}
		fonts[name] = f;
		return &fonts[name];
	} else {
		return &it->second;
	}
}

sf::Image *ResourceManager::loadImage(string name){
	map<string, sf::Image>::iterator it = images.find(name);
	if(it == images.end()){
		sf::Image i;
		if(i.loadFromFile(imgDir+name) == false){
			return NULL;
		}
		images[name] = i;
		return &images[name];
	} else {
		return &it->second;
	}
}
sf::SoundBuffer *ResourceManager::loadSound(string name){
	map<string, sf::SoundBuffer>::iterator it = sounds.find(name);
	if(it == sounds.end()){
		sf::SoundBuffer b;
		if(b.loadFromFile(soundDir+name) == false){
			return NULL;
		}
		sounds[name] = b;
		return &sounds[name];
	} else {
		return &it->second;
	}
}
Texture ResourceManager::loadTexture(string name){
	if(name.length() == 0){
		Texture error;
		error.id = -1;
		cerr << "Texture name is empty." << endl;
		return error;
	}
	map<string, Texture>::iterator it = texs.find(name);
	if(it == texs.end()){
		GLuint i;
		sf::Vector2i size;
		sf::Image *img = loadImage(name);
		if(img == NULL){
			Texture error;
			error.id = -1;
			return error;
		}
		size.x = img->getSize().x;
		size.y = img->getSize().y;
		glGenTextures(1, &i);
		glBindTexture(GL_TEXTURE_2D, i);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D,
				0,
				GL_RGBA,
				size.x,
				size.y,
				0,
				GL_RGBA,
				GL_UNSIGNED_BYTE,
				img->getPixelsPtr());
		cout << "IMAGE X:"<<img->getSize().x << " IMAGE Y:" << img->getSize().y << endl;
		Texture out;
		out.height = size.y;
		out.width = size.x;
		out.id = i;
		texs[name] = out;
		return texs[name];
	} else {
		return it->second;
	}
}
