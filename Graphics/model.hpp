#ifndef MODEL_HPP
#define MODEL_HPP

#include <GL/glew.h>
#include <iostream>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/norm.hpp>
#include "iqm.hpp"
#include "GraphicsUtils.hpp"
#include "Texture.hpp"

struct extents{
	float minx;
	float maxx;
	float miny;
	float maxy;
	float minz;
	float maxz;
};

struct vertex{
    GLfloat position[3];
    GLfloat normal[3];
    GLfloat tangent[4];
    GLfloat texcoord[2];
    GLubyte blendindex[4];
    GLubyte blendweight[4];
};

class Model {
	private:
		GLuint tris_ebo;
		GLuint verts_vbo;	
		GLuint textureID;
	public:
		std::vector<vertex> verts;
		std::vector<iqmtriangle> triangles;
		std::vector<iqmmesh> meshes;
		std::vector<iqmjoint> joints;
		std::vector<Texture> textures;
		std::vector<glm::mat4> baseframe;
		std::vector<glm::mat4> inversebaseframe;
		std::vector<iqmanim> anims;
		std::vector<std::string> animNames;
		std::vector<iqmpose> poses;
		std::vector<glm::mat4> frames;
		extents e;
		int numFrames;

		void setEBO(GLuint ebo);
		void setVBO(GLuint vbo);
		void setTEXID(GLuint id);

		void draw(ShaderProgram *prg, std::vector<Texture> textures,std::vector<glm::mat4> outframe, bool texture, bool normal);
		void animate(std::string animName, float currentframe, std::vector<glm::mat4> *outframe);
		void getExtents();
};

bool loadIQM(std::string filename, Model &target);
bool loadIQMMesh(std::string filename, iqmheader header, 
		Model &target ,unsigned char *buf, bool GL);
bool loadIQMAnim(std::string filename, iqmheader header,
		Model &target, unsigned char *buf);

bool noGLLoadIQM(std::string filename, Model &target);



#endif
