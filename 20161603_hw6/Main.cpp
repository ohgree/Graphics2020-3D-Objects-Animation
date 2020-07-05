#define _CRT_SECURE_NO_WARNINGS

#include "my_geom_objects.h"

// include glm/*.hpp only if necessary
//#include <glm/glm.hpp> 
#include <glm/gtc/matrix_transform.hpp> //translate, rotate, scale, lookAt, perspective, etc.
#include <glm/gtc/matrix_inverse.hpp> // inverseTranspose, etc.

typedef struct _Camera {
	glm::vec3 eye;
	glm::vec3 to;
	glm::vec3 up;
	glm::mat3 coordinate;
} Camera;

Camera camera = {
	glm::vec3(300.0f, 400.0f, 300.0f),
	glm::vec3(0.0f, 0.0f, 0.0f),
	glm::vec3(0.0f, 1.0f, 0.0f),
	glm::mat3(1.0f)
};

glm::mat3 get_camera_coordinate(Camera cam) {
	return glm::transpose(glm::mat3(glm::lookAt(cam.eye, cam.to, cam.up)));
}

glm::mat4 ModelViewProjectionMatrix, ModelViewMatrix;
glm::mat3 ModelViewMatrixInvTrans;
glm::mat4 ViewMatrix, ProjectionMatrix;

#define TO_RADIAN 0.01745329252f  
#define TO_DEGREE 57.295779513f

// lights in scene
Light_Parameters light[NUMBER_OF_LIGHT_SUPPORTED];

float rotation_angle_tiger = 0.0f;
float rotation_angle = 0.0f;
int width, height;

#define TEXTURE_ID_GRASS 6
#define TEXTURE_ID_WOOD 7
void prepare_grass(void) {
	glHint(GL_GENERATE_MIPMAP_HINT, GL_NICEST);

	glActiveTexture(GL_TEXTURE0 + TEXTURE_ID_GRASS);
	glBindTexture(GL_TEXTURE_2D, texture_names[TEXTURE_ID_GRASS]);

	My_glTexImage2D_from_file("Data/static_objects/grass_tex.jpg");

	glGenerateMipmap(GL_TEXTURE_2D);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
}
void prepare_my_texture(void) {
	glHint(GL_GENERATE_MIPMAP_HINT, GL_NICEST);

	glActiveTexture(GL_TEXTURE0 + TEXTURE_ID_WOOD);
	glBindTexture(GL_TEXTURE_2D, texture_names[TEXTURE_ID_WOOD]);

	My_glTexImage2D_from_file("Data/static_objects/my_tex.jpg");

	glGenerateMipmap(GL_TEXTURE_2D);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

glm::mat4 TankViewMatrix = glm::mat4(1.0f);
glm::mat3 TankViewMatrixInvTrans = glm::mat3(1.0f);
void refresh_dynamic_lights(void) {

	for (int shader_default = 0 ; shader_default < NUMBER_OF_SHADERS; shader_default++) {
		glUseProgram(h_ShaderProgram[shader_default]);

		// Light source #0: parallel_light_WC
		// need to convert position to EC for shading. Rigid body transform, thus no need for InvTrans
		glm::vec4 position_EC_0 = ViewMatrix * glm::vec4(
			light[1].position[0], light[1].position[1],
			light[1].position[2], light[1].position[3]
		);
		glUniform4fv(loc_light[shader_default][0].position, 1, &position_EC_0[0]);

		// Light source #1: spot_light_WC
		// need to convert position to EC for shading
		glm::vec4 position_EC_1 = ViewMatrix * glm::vec4(
			light[1].position[0], light[1].position[1],
			light[1].position[2], light[1].position[3]
		);
		glUniform4fv(loc_light[shader_default][1].position, 1, &position_EC_1[0]);
		// need to supply direction in EC for shading in this example shader
		// note that the viewing transform is a rigid body transform
		// thus transpose(inverse(mat3(ViewMatrix)) = mat3(ViewMatrix)
		glm::vec3 direction_EC_1 = glm::mat3(ViewMatrix) * glm::vec3(
			light[1].spot_direction[0],
			light[1].spot_direction[1],
			light[1].spot_direction[2]
		);
		glUniform3fv(loc_light[shader_default][1].spot_direction, 1, &direction_EC_1[0]);

		/**
		 * Dynamic light: light3 - Tank spotlight(MC)
		 */
		 // position is in MC, so ModelView transformation is required
		glm::vec4 position_EC_3 = TankViewMatrix * glm::vec4(
			light[3].position[0],
			light[3].position[1],
			light[3].position[2],
			light[3].position[3]
		);
		glUniform4fv(loc_light[shader_default][3].position, 1, &position_EC_3[0]);
		// direction in EC is in MC, needs ModelView transformation (rigid body trans). No invTrans required.
		glm::vec3 direction_EC_3 = glm::vec3(TankViewMatrixInvTrans * glm::vec3(
			light[3].spot_direction[0],
			light[3].spot_direction[1],
			light[3].spot_direction[2]
		));
		//fprintf(stdout, "tank direction: %f %f %f\n", direction_EC_3[0], direction_EC_3[1], direction_EC_3[2]);
		glUniform3fv(loc_light[shader_default][3].spot_direction, 1, &direction_EC_3[0]);
	}
	glUseProgram(0);
}

#pragma region callbacks
// callbacks

bool texture_toggle = false;
bool screen = false;
bool extra = false;
int floor_tiles = 1;

#define FLOOR_TILE_NUMBER 8
#define DRAGON_ROLL_DEGREES 30.0f
#define DRAGON_ANGLE_MULTIPLIER 1.0f
#define DRAGON_MAX_HEIGHT 100.0f
#define WOLF_OFFSET_DEGREES 60.0f
#define WOLF_JUMP_FREQUENCY_MULTIPLIER 8.0f
#define WOLF_JUMP_HEIGHT 70.0f
#define SPIDER_MAX_OFFSET 100.0f
void display(void) {
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glUseProgram(h_ShaderProgram_simple);
	ModelViewMatrix = glm::scale(ViewMatrix, glm::vec3(50.0f, 50.0f, 50.0f));
	ModelViewProjectionMatrix = ProjectionMatrix * ModelViewMatrix;
	glUniformMatrix4fv(loc_ModelViewProjectionMatrix_simple, 1, GL_FALSE, &ModelViewProjectionMatrix[0][0]);
	glLineWidth(2.0f);
	draw_axes();
	glLineWidth(1.0f);

	/**
	 * floor(static object) with grass texture, tiled
	 */
	glUseProgram(h_ShaderProgram[shader_default]);
  	set_material_wolf();
	glUniform1i(loc_number_of_tiles[shader_default], extra ? floor_tiles : FLOOR_TILE_NUMBER);
	glUniform1i(loc_texture[shader_default], texture_toggle ? TEXTURE_ID_WOOD : TEXTURE_ID_GRASS);
	ModelViewMatrix = glm::translate(ViewMatrix, glm::vec3(-500.0f, 0.0f, 500.0f));
	ModelViewMatrix = glm::scale(ModelViewMatrix, glm::vec3(1000.0f, 1000.0f, 1000.0f));
	ModelViewMatrix = glm::rotate(ModelViewMatrix, -90.0f*TO_RADIAN, glm::vec3(1.0f, 0.0f, 0.0f));
	ModelViewProjectionMatrix = ProjectionMatrix * ModelViewMatrix;
	ModelViewMatrixInvTrans = glm::inverseTranspose(glm::mat3(ModelViewMatrix));

	glUniformMatrix4fv(loc_ModelViewProjectionMatrix[shader_default], 1, GL_FALSE, &ModelViewProjectionMatrix[0][0]);
	glUniformMatrix4fv(loc_ModelViewMatrix[shader_default], 1, GL_FALSE, &ModelViewMatrix[0][0]);
	glUniformMatrix3fv(loc_ModelViewMatrixInvTrans[shader_default], 1, GL_FALSE, &ModelViewMatrixInvTrans[0][0]);
	glUniform1i(loc_u_screen, screen);
	draw_floor();
	glUniform1i(loc_number_of_tiles[shader_default], 1);
	glUniform1i(loc_u_screen, false);

	/**
	 * dragon(static object) - rotating with corresponding roll value, its initial path is left-handed helix
	 */
	set_material_ben();
	glUniform1i(loc_texture[shader_default], TEXTURE_ID_WOLF);
	glm::mat4 DragonPosMatrix = glm::rotate(glm::mat4(1.0f),
		-rotation_angle * DRAGON_ANGLE_MULTIPLIER * TO_RADIAN, glm::vec3(0.0f, 1.0f, 0.0f));	
	DragonPosMatrix = glm::translate(DragonPosMatrix, glm::vec3(0.0f,
			DRAGON_MAX_HEIGHT * abs(sin(0.5f * rotation_angle * TO_RADIAN)), -200.0f));
	DragonPosMatrix = glm::rotate(DragonPosMatrix, abs(sin(0.5f * rotation_angle * TO_RADIAN)), glm::vec3(1.0f, 0.0f, 0.0f));
	ModelViewMatrix = glm::rotate(ViewMatrix * DragonPosMatrix, (-90.0f) * TO_RADIAN, glm::vec3(1.0f, 0.0f, 0.0f));
	ModelViewMatrix = glm::scale(ModelViewMatrix, glm::vec3(6.0f));
	ModelViewProjectionMatrix = ProjectionMatrix * ModelViewMatrix;
	ModelViewMatrixInvTrans = glm::inverseTranspose(glm::mat3(ModelViewMatrix));

	glUniformMatrix4fv(loc_ModelViewProjectionMatrix[shader_default], 1, GL_FALSE, &ModelViewProjectionMatrix[0][0]);
	glUniformMatrix4fv(loc_ModelViewMatrix[shader_default], 1, GL_FALSE, &ModelViewMatrix[0][0]);
	glUniformMatrix3fv(loc_ModelViewMatrixInvTrans[shader_default], 1, GL_FALSE, &ModelViewMatrixInvTrans[0][0]);
	draw_dragon();

	/** 
	 * wolf(dynamic obj) - chasing the dragon while jumping
	 */
	set_material_wolf();
	glUniform1i(loc_texture[shader_default], TEXTURE_ID_WOLF);
	ModelViewMatrix = glm::rotate(ViewMatrix, 
		(WOLF_OFFSET_DEGREES - rotation_angle * DRAGON_ANGLE_MULTIPLIER) * TO_RADIAN, glm::vec3(0.0f, 1.0f, 0.0f));
	ModelViewMatrix = glm::translate(ModelViewMatrix, glm::vec3(
		0.0f, glm::max(0.0f, WOLF_JUMP_HEIGHT * sin(WOLF_JUMP_FREQUENCY_MULTIPLIER * rotation_angle * TO_RADIAN)), -200.0f)
	);
	ModelViewMatrix = glm::rotate(ModelViewMatrix, 90.0f * TO_RADIAN, glm::vec3(0.0f, 1.0f, 0.0f));
	ModelViewMatrix = glm::scale(ModelViewMatrix, glm::vec3(80.0f));
	ModelViewProjectionMatrix = ProjectionMatrix * ModelViewMatrix;
	ModelViewMatrixInvTrans = glm::inverseTranspose(glm::mat3(ModelViewMatrix));

	glUniformMatrix4fv(loc_ModelViewProjectionMatrix[shader_default], 1, GL_FALSE, &ModelViewProjectionMatrix[0][0]);
	glUniformMatrix4fv(loc_ModelViewMatrix[shader_default], 1, GL_FALSE, &ModelViewMatrix[0][0]);
	glUniformMatrix3fv(loc_ModelViewMatrixInvTrans[shader_default], 1, GL_FALSE, &ModelViewMatrixInvTrans[0][0]);
	draw_wolf();

	/**
	 * spider(dynamic obj) - eyes on dragon
	 */
	set_material_ben();
	glUniform1i(loc_texture[shader_default], TEXTURE_ID_SPIDER);
	ModelViewMatrix = glm::rotate(ViewMatrix, (-rotation_angle * DRAGON_ANGLE_MULTIPLIER + 180) * TO_RADIAN, glm::vec3(0.0f, 1.0f, 0.0f));
	ModelViewMatrix = glm::scale(ModelViewMatrix, glm::vec3(50.0f, -50.0f, 50.0f));
	ModelViewProjectionMatrix = ProjectionMatrix * ModelViewMatrix;
	ModelViewMatrixInvTrans = glm::inverseTranspose(glm::mat3(ModelViewMatrix));

	glUniformMatrix4fv(loc_ModelViewProjectionMatrix[shader_default], 1, GL_FALSE, &ModelViewProjectionMatrix[0][0]);
	glUniformMatrix4fv(loc_ModelViewMatrix[shader_default], 1, GL_FALSE, &ModelViewMatrix[0][0]);
	glUniformMatrix3fv(loc_ModelViewMatrixInvTrans[shader_default], 1, GL_FALSE, &ModelViewMatrixInvTrans[0][0]);
	draw_spider();

	/**
	 * tiger(dynamic obj) - carried by dragon with its fang
	 */

 	set_material_tiger();
	glUniform1i(loc_texture[shader_default], TEXTURE_ID_TIGER);
	ModelViewMatrix = glm::translate(ViewMatrix*DragonPosMatrix, glm::vec3(85.0f, 70.0f, 10.0f));
	ModelViewMatrix = glm::rotate(ModelViewMatrix, 80.0f * TO_RADIAN, glm::vec3(0.0f, 0.0f, 1.0f));
	ModelViewMatrix = glm::rotate(ModelViewMatrix, -90.0f*TO_RADIAN, glm::vec3(1.0f, 0.0f, 0.0f));
	ModelViewMatrix = glm::scale(ModelViewMatrix, glm::vec3(0.3f));
	ModelViewProjectionMatrix = ProjectionMatrix * ModelViewMatrix;
	ModelViewMatrixInvTrans = glm::inverseTranspose(glm::mat3(ModelViewMatrix));

	glUniformMatrix4fv(loc_ModelViewProjectionMatrix[shader_default], 1, GL_FALSE, &ModelViewProjectionMatrix[0][0]);
	glUniformMatrix4fv(loc_ModelViewMatrix[shader_default], 1, GL_FALSE, &ModelViewMatrix[0][0]);
	glUniformMatrix3fv(loc_ModelViewMatrixInvTrans[shader_default], 1, GL_FALSE, &ModelViewMatrixInvTrans[0][0]);
	draw_tiger();

	/**
	 * tank(static obj) - facing toward the dragon.
	 * Used inverse of glm::lookAt to simply calculate the transformation.
	 */
	set_material_tank();
	glUniform1i(loc_texture[shader_default], TEXTURE_ID_TANK);
	glm::vec3 dragon_pos = glm::vec3(DragonPosMatrix * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
	ModelViewMatrix = ViewMatrix * glm::inverse(glm::lookAt(
		glm::vec3(200.0f, 0.0f, -200.0f), dragon_pos, glm::vec3(0.0f, 1.0f, 0.0f)
	));
	ModelViewMatrix = glm::rotate(ModelViewMatrix, 180.0f * TO_RADIAN, glm::vec3(0.0f, 1.0f, 0.0f));
	ModelViewMatrix = glm::rotate(ModelViewMatrix, -90.0f * TO_RADIAN, glm::vec3(1.0f, 0.0f, 0.0f));
	ModelViewMatrix = glm::scale(ModelViewMatrix, glm::vec3(10.0f));
	ModelViewMatrix = glm::translate(ModelViewMatrix, glm::vec3(0.0f, -15.0f, 0.0f));
	TankViewMatrix = ModelViewMatrix;

	ModelViewProjectionMatrix = ProjectionMatrix * ModelViewMatrix;
	ModelViewMatrixInvTrans = glm::inverseTranspose(glm::mat3(ModelViewMatrix));
	TankViewMatrixInvTrans = ModelViewMatrixInvTrans;

	glUniformMatrix4fv(loc_ModelViewProjectionMatrix[shader_default], 1, GL_FALSE, &ModelViewProjectionMatrix[0][0]);
	glUniformMatrix4fv(loc_ModelViewMatrix[shader_default], 1, GL_FALSE, &ModelViewMatrix[0][0]);
	glUniformMatrix3fv(loc_ModelViewMatrixInvTrans[shader_default], 1, GL_FALSE, &ModelViewMatrixInvTrans[0][0]);
	draw_tank();

	refresh_dynamic_lights();

	glUseProgram(0);

	glutSwapBuffers();
}


bool fun = false;
float spotlight_modifier = 0.0f;

void timer_scene(int value) {
	timestamp_scene = (timestamp_scene + 1) % UINT_MAX;
	cur_frame_tiger = timestamp_scene % N_TIGER_FRAMES;
	//cur_frame_ben = timestamp_scene % N_BEN_FRAMES;
	cur_frame_wolf= timestamp_scene % N_WOLF_FRAMES;
	cur_frame_spider = timestamp_scene % N_SPIDER_FRAMES;
	rotation_angle = (float)(timestamp_scene % 360);
	if (extra) {
		floor_tiles = (timestamp_scene / 20) % FLOOR_TILE_NUMBER + 1;
		if (timestamp_scene % 10 == 0) {
			screen = !screen;
		}
		if (timestamp_scene % 5 == 0) {
			texture_toggle = !texture_toggle;
		}
	}
	if (fun) {
		spotlight_modifier = 1440 * glm::sin((timestamp_scene % (360*30)) / 30.0f);
		glUseProgram(h_ShaderProgram[PHONG]);
		glUniform1f(loc_u_spotlight_modifier, spotlight_modifier);
	}
	glutPostRedisplay();
	glutTimerFunc(10, timer_scene, 0);
}

#define CAMERA_MOVEMENT_SPEED 10.0f
void keyboard(unsigned char key, int x, int y) {
	static int flag_cull_face = 0;
	static int PRP_distance_level = 4;

	glm::vec4 position_EC;
	glm::vec3 direction_EC;

	if ((key >= '0') && (key <= '0' + NUMBER_OF_LIGHT_SUPPORTED - 1)) {
		int light_ID = (int)(key - '0');
		light[light_ID].light_on = 1 - light[light_ID].light_on;
		for (int i = 0; i < NUMBER_OF_SHADERS; i++) {
			glUseProgram(h_ShaderProgram[i]);
			glUniform1i(loc_light[i][light_ID].light_on, light[light_ID].light_on);
			glUseProgram(0);
		}

		glutPostRedisplay();
		fprintf(stdout, "Light source %d is toggled: %d\n", light_ID, light[light_ID].light_on);
		return;
	}
	glm::vec3 offset;
	switch (key) {
	case 't':
		texture_toggle = !texture_toggle;
		glutPostRedisplay();
		break;
		/*
	case 'c':
		flag_cull_face = (flag_cull_face + 1) % 3;
		switch (flag_cull_face) {
		case 0:
			glDisable(GL_CULL_FACE);
			glutPostRedisplay();
			break;
		case 1: // cull back faces;
			glCullFace(GL_BACK);
			glEnable(GL_CULL_FACE);
			glutPostRedisplay();
			break;
		case 2: // cull front faces;
			glCullFace(GL_FRONT);
			glEnable(GL_CULL_FACE);
			glutPostRedisplay();
			break;
		}
		break;
		*/
	case 'i':
	case 'k':
		//i: forward(-n), k: backward(+n)
		offset = CAMERA_MOVEMENT_SPEED * camera.coordinate[2];
		camera.eye += (key == 'k') ? offset : -offset;
		camera.to = camera.eye - camera.coordinate[2];
		ViewMatrix = glm::lookAt(camera.eye, camera.to, camera.up);
		refresh_dynamic_lights();
		glutPostRedisplay();
		break;
	case 'j':
	case 'l':
		//left(-u), right(+u)
		offset = CAMERA_MOVEMENT_SPEED * camera.coordinate[0];
		camera.eye += (key == 'j') ? -offset : offset;
		camera.to = camera.eye - camera.coordinate[2];
		ViewMatrix = glm::lookAt(camera.eye, camera.to, camera.up);
		refresh_dynamic_lights();
		glutPostRedisplay();
		break;
	case 'u':
	case 'o':
		//up(+v), down(-v)
		offset = CAMERA_MOVEMENT_SPEED * camera.coordinate[1];
		camera.eye += (key == 'u') ? offset : -offset;
		camera.to = camera.eye - camera.coordinate[2];
		ViewMatrix = glm::lookAt(camera.eye, camera.to, camera.up);
		refresh_dynamic_lights();
		glutPostRedisplay();
		break;
	case 'q':
		// toggle shader
		if (shader_default) {
			shader_default = PHONG;
			fprintf(stdout, "Toggled default shader to Phong Shader\n");
		} else {
			shader_default = GOURAUD;
			fprintf(stdout, "Toggled default shader to Gouraud Shader\n");
		}
		glutPostRedisplay();
		break;
	case 'w':
		fun = !fun;
		glUseProgram(h_ShaderProgram[PHONG]);
		glUniform1i(loc_u_fun, fun);
		if (fun) {
			spotlight_modifier = 0.0f;
			glUniform1f(loc_u_spotlight_modifier, spotlight_modifier);
		}
		glUseProgram(0);
		glutPostRedisplay();
		fprintf(stdout, "Toggled spotlight psychedelic effect\n");
		break;
	case 'e':
		screen = !screen;
		glutPostRedisplay();
		fprintf(stdout, "Toggled screen effect for floor\n");
		break;
	case 'r':
		extra = !extra;
		if (!extra)
			screen = texture_toggle = false;
		glutPostRedisplay();
		fprintf(stdout, "Toggled extra feature\n");
		break;
	case 27: // ESC key
		glutLeaveMainLoop(); // Incur destuction callback for cleanups
		break;
	}
}

bool lmb_hold = false;

void mouse(int button, int state, int x, int y) {
	if (button == GLUT_LEFT_BUTTON) {
		if (state == GLUT_DOWN) {
			lmb_hold = true;
			glutWarpPointer(width / 2, height / 2);
			fprintf(stdout, "LMB pressed\n");
			glutPostRedisplay();
		}
		else if (state == GLUT_UP) {
			lmb_hold = false;
			fprintf(stdout, "LMB up\n");
			glutPostRedisplay();
		}
	}
}

#define CAMERA_ROLL 10.0f * TO_RADIAN

void special(int key, int x, int y) {
	if (key != GLUT_KEY_LEFT && key != GLUT_KEY_RIGHT) 
		return;

	// roll camera
	camera.coordinate = glm::mat3(glm::transpose(glm::rotate(glm::transpose(glm::mat4(camera.coordinate)),
		(key == GLUT_KEY_RIGHT) ? CAMERA_ROLL : -CAMERA_ROLL, camera.coordinate[2])));
	camera.to = camera.eye - camera.coordinate[2];
	camera.up = camera.coordinate[1];
	ViewMatrix = glm::lookAt(camera.eye, camera.to, camera.up);
	refresh_dynamic_lights();
	glutPostRedisplay();
}

#define CAMERA_ZOOM_SENSITIVITY 5.0f * TO_RADIAN
#define FOV_LOWER_BOUND 30.0f * TO_RADIAN
#define FOV_UPPER_BOUND 105.0f * TO_RADIAN
float fov = 60.0f * TO_RADIAN;
void wheel(int wheel, int direction, int x, int y) {
	float fov_offset = -direction * CAMERA_ZOOM_SENSITIVITY;
	fov = glm::min(glm::max(fov + fov_offset, FOV_LOWER_BOUND), FOV_UPPER_BOUND);
	ProjectionMatrix = glm::perspective(fov, (float)width/height, 100.0f, 20000.0f);
	glutPostRedisplay();
}

#define MOUSE_SENSITIVITY 0.035f

void motion(int x, int y) {
	if (!lmb_hold) return;
	int delx, dely;
	delx = x - width / 2;
	dely = y - height / 2;
	
	camera.coordinate = glm::transpose(glm::mat3(glm::rotate(glm::mat4(glm::transpose(camera.coordinate)), 
		MOUSE_SENSITIVITY * dely * TO_RADIAN, camera.coordinate[0])));
	
	camera.coordinate = glm::transpose(glm::mat3(glm::rotate(
		glm::mat4(glm::transpose(camera.coordinate)),
		MOUSE_SENSITIVITY * delx * TO_RADIAN,
		glm::dot(glm::vec3(0.0f, 1.0f, 0.0f), camera.coordinate[1]) >= 0 ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(0.0f, -1.0f, 0.0f)
	)));

	camera.to = camera.eye - camera.coordinate[2];
	camera.up = camera.coordinate[1];
	ViewMatrix = glm::lookAt(camera.eye, camera.to, camera.up);
	refresh_dynamic_lights();
	glutWarpPointer(width / 2, height / 2);
	glutPostRedisplay();
}

void reshape(int width, int height) {
	::width = width;
	::height = height;
	glViewport(0, 0, width, height);
	float aspect_ratio = (float) width / height;
	ProjectionMatrix = glm::perspective(fov, aspect_ratio, 100.0f, 20000.0f);
	glutPostRedisplay();
}

void cleanup(void) {
	glDeleteVertexArrays(1, &axes_VAO);
	glDeleteBuffers(1, &axes_VBO);

	glDeleteVertexArrays(1, &rectangle_VAO);
	glDeleteBuffers(1, &rectangle_VBO);

	glDeleteVertexArrays(1, &tiger_VAO);
	glDeleteBuffers(1, &tiger_VBO);

	glDeleteTextures(N_TEXTURES_USED, texture_names);
}
#pragma endregion

void register_callbacks(void) {
	glutDisplayFunc(display);
	glutKeyboardFunc(keyboard);
	glutReshapeFunc(reshape);
	glutMouseFunc(mouse);
	glutMouseWheelFunc(wheel);
	glutMotionFunc(motion);
	glutSpecialFunc(special);
	glutTimerFunc(100, timer_scene, 0);
	glutCloseFunc(cleanup);
}

void initialize_OpenGL(void) {

	glEnable(GL_MULTISAMPLE);
  	glEnable(GL_DEPTH_TEST);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	glClearColor(0.5f, 0.7f, 1.0f, 1.0f);

	ViewMatrix = glm::lookAt(camera.eye, camera.to, camera.up);
	camera.coordinate = get_camera_coordinate(camera);
	initialize_lights_and_material();
	initialize_flags();

	glGenTextures(N_TEXTURES_USED, texture_names);

}

void set_up_scene_lights(void) {

	/**
	 * light 0: parallel light in WC
	 * light colour: F1DAA4 (sunlight)
	 */
	light[0].light_on = 1;
	light[0].position[0] = 1.0f; light[0].position[1] = 1.0f; // parallel light direction in WC: point at infinity
	light[0].position[2] = 1.0f; light[0].position[3] = 0.0f;

	// light colour
	light[0].ambient_color[0] = 0.09f; light[0].ambient_color[1] = 0.08f;
	light[0].ambient_color[2] = 0.06f; light[0].ambient_color[3] = 1.0f;

	light[0].diffuse_color[0] = 0.47f; light[0].diffuse_color[1] = 0.42f;
	light[0].diffuse_color[2] = 0.32f; light[0].diffuse_color[3] = 1.0f;

	light[0].specular_color[0] = 0.95f; light[0].specular_color[1] = 0.85f;
	light[0].specular_color[2] = 0.64f; light[0].specular_color[3] = 1.0f;


	/**
	 * light 1: spotlight in WC
	 */
	light[1].light_on = 1;
	light[1].position[0] = -250.0f; light[1].position[1] = 500.0f; // spot light position in WC
	light[1].position[2] = -250.0f; light[1].position[3] = 1.0f;

	light[1].ambient_color[0] = 0.1f; light[1].ambient_color[1] = 0.1f;
	light[1].ambient_color[2] = 0.1f; light[1].ambient_color[3] = 1.0f;

	light[1].diffuse_color[0] = 0.5f; light[1].diffuse_color[1] = 0.5f;
	light[1].diffuse_color[2] = 0.5f; light[1].diffuse_color[3] = 1.0f;

	light[1].specular_color[0] = 0.5f; light[1].specular_color[1] = 0.5f;
	light[1].specular_color[2] = 0.5f; light[1].specular_color[3] = 1.0f;

	// spot light direction in WC: (0.5, -1.0, 0.0)
	light[1].spot_direction[0] = 0.5f; 
	light[1].spot_direction[1] = -1.0f;
	light[1].spot_direction[2] = 0.0f;
	light[1].spot_cutoff_angle = 20.0f;
	light[1].spot_exponent = 32.0f;


	/**
	 * light 2: spotlight in EC
	 * position: (0, 0, 0) at EC
	 * direction: -n (viewing direction)
	 * colour: magenta-based
	 */
	light[2].light_on = 0;
	light[2].position[0] = 0.0f; light[2].position[1] = 0.0f; 	// point light position in EC
	light[2].position[2] = 0.0f; light[2].position[3] = 1.0f;

	light[2].ambient_color[0] = 0.13f; light[2].ambient_color[1] = 0.0f;
	light[2].ambient_color[2] = 0.13f; light[2].ambient_color[3] = 1.0f;

	light[2].diffuse_color[0] = 0.8f; light[2].diffuse_color[1] = 0.0f;
	light[2].diffuse_color[2] = 0.8f; light[2].diffuse_color[3] = 1.0f;

	light[2].specular_color[0] = 0.8f; light[2].specular_color[1] = 0.0f;
	light[2].specular_color[2] = 0.8f; light[2].specular_color[3] = 1.0f;

	// spot light direction in EC: (0.0, 0.0, -1.0) = -n
	light[2].spot_direction[0] = 0.0f;
	light[2].spot_direction[1] = 0.0f;
	light[2].spot_direction[2] = -1.0f;
	light[2].spot_cutoff_angle = 10.0f;
	light[2].spot_exponent = 16.0f;

	/**
	 * light 3: spotlight in MC (Tank)
	 * position: Tank Artillery
	 * direction: Tank's view (0,-1,0 in MC)
	 * colour: white
	 */
	light[3].light_on = 0;
	light[3].position[0] = 0.0f; light[3].position[1] = 20.0f; 	// point light position in MC
	light[3].position[2] = 5.0f; light[3].position[3] = 1.0f;

	light[3].ambient_color[0] = 0.1f; light[3].ambient_color[1] = 0.1f;
	light[3].ambient_color[2] = 0.1f; light[3].ambient_color[3] = 1.0f;

	light[3].diffuse_color[0] = 1.0f; light[3].diffuse_color[1] = 1.0f;
	light[3].diffuse_color[2] = 1.0f; light[3].diffuse_color[3] = 1.0f;

	light[3].specular_color[0] = 1.0f; light[3].specular_color[1] = 1.0f;
	light[3].specular_color[2] = 1.0f; light[3].specular_color[3] = 1.0f;

	// spot light direction in MC: (0.0, -1.0, 0.0) = -y
	light[3].spot_direction[0] = 0.0f;
	light[3].spot_direction[1] = -1.0f;
	light[3].spot_direction[2] = 0.0f;
	light[3].spot_cutoff_angle = 30.0f;
	light[3].spot_exponent = 8.0f;


	for (int shader_default = 0; shader_default < NUMBER_OF_SHADERS; shader_default++) {
		glUseProgram(h_ShaderProgram[shader_default]);

		// Light source #0: parallel_light_WC
		glUniform1i(loc_light[shader_default][0].light_on, light[0].light_on);
		// need to convert position to EC for shading. Rigid body transform, thus no need for InvTrans
		glm::vec4 position_EC_0 = ViewMatrix * glm::vec4(
			light[1].position[0], light[1].position[1],
			light[1].position[2], light[1].position[3]
		);
		glUniform4fv(loc_light[shader_default][0].position, 1, &position_EC_0[0]);
		glUniform4fv(loc_light[shader_default][0].ambient_color, 1, light[0].ambient_color);
		glUniform4fv(loc_light[shader_default][0].diffuse_color, 1, light[0].diffuse_color);
		glUniform4fv(loc_light[shader_default][0].specular_color, 1, light[0].specular_color);

		// Light source #1: spot_light_WC
		glUniform1i(loc_light[shader_default][1].light_on, light[1].light_on);
		// need to convert position to EC for shading
		glm::vec4 position_EC_1 = ViewMatrix * glm::vec4(
			light[1].position[0], light[1].position[1],
			light[1].position[2], light[1].position[3]
		);
		glUniform4fv(loc_light[shader_default][1].position, 1, &position_EC_1[0]);
		glUniform4fv(loc_light[shader_default][1].ambient_color, 1, light[1].ambient_color);
		glUniform4fv(loc_light[shader_default][1].diffuse_color, 1, light[1].diffuse_color);
		glUniform4fv(loc_light[shader_default][1].specular_color, 1, light[1].specular_color);
		// need to supply direction in EC for shading in this example shader
		// note that the viewing transform is a rigid body transform
		// thus transpose(inverse(mat3(ViewMatrix)) = mat3(ViewMatrix)
		glm::vec3 direction_EC_1 = glm::mat3(ViewMatrix) * glm::vec3(
			light[1].spot_direction[0],
			light[1].spot_direction[1],
			light[1].spot_direction[2]
		);
		glUniform3fv(loc_light[shader_default][1].spot_direction, 1, &direction_EC_1[0]);
		glUniform1f(loc_light[shader_default][1].spot_cutoff_angle, light[1].spot_cutoff_angle);
		glUniform1f(loc_light[shader_default][1].spot_exponent, light[1].spot_exponent);


		// Light source #2: spot_light_EC
		glUniform1i(loc_light[shader_default][2].light_on, light[2].light_on);
		// position is already in EC
		glUniform4fv(loc_light[shader_default][2].position, 1, &light[2].position[0]);
		glUniform4fv(loc_light[shader_default][2].ambient_color, 1, light[2].ambient_color);
		glUniform4fv(loc_light[shader_default][2].diffuse_color, 1, light[2].diffuse_color);
		glUniform4fv(loc_light[shader_default][2].specular_color, 1, light[2].specular_color);
		// direction in EC is as it is.
		glUniform3fv(loc_light[shader_default][2].spot_direction, 1, &light[2].spot_direction[0]);
		glUniform1f(loc_light[shader_default][2].spot_cutoff_angle, light[2].spot_cutoff_angle);
		glUniform1f(loc_light[shader_default][2].spot_exponent, light[2].spot_exponent);


		// Light source #3: spot_light_MC
		glUniform1i(loc_light[shader_default][3].light_on, light[3].light_on);

		glUniform4fv(loc_light[shader_default][3].ambient_color, 1, light[3].ambient_color);
		glUniform4fv(loc_light[shader_default][3].diffuse_color, 1, light[3].diffuse_color);
		glUniform4fv(loc_light[shader_default][3].specular_color, 1, light[3].specular_color);

		glUniform1f(loc_light[shader_default][3].spot_cutoff_angle, light[3].spot_cutoff_angle);
		glUniform1f(loc_light[shader_default][3].spot_exponent, light[3].spot_exponent);

		refresh_dynamic_lights();


		glUseProgram(0);
	}
}

void prepare_scene(void) {
	prepare_axes();
	prepare_floor();
	prepare_tiger();
	prepare_wolf();
	prepare_spider();
	prepare_dragon();
	prepare_optimus();
	prepare_cow();
	prepare_bus();
	prepare_bike();
	prepare_godzilla();
	prepare_ironman();
	prepare_tank();
	set_up_scene_lights();

	prepare_grass();
	prepare_my_texture();
}

void initialize_renderer(void) {
	register_callbacks();
	prepare_shader_program();
	initialize_OpenGL();
	prepare_scene();
}

#define N_MESSAGE_LINES 1
void main(int argc, char *argv[]) {
	char program_name[64] = "Sogang CSE4170 HW6 - 20161603 MJ Shin";
	char messages[N_MESSAGE_LINES][256] = { "    - Keys used: '0', '1', 'a', 't', 'f', 'c', 'd', 'y', 'u', 'i', 'o', 'ESC'"  };

	glutInit(&argc, argv);
  	glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH | GLUT_MULTISAMPLE);
	glutInitWindowSize(1200, 800);
	glutInitContextVersion(3, 3);
	glutInitContextProfile(GLUT_CORE_PROFILE);
	glutCreateWindow(program_name);

	greetings(program_name, messages, N_MESSAGE_LINES);
	initialize_renderer();

	glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS);
	glutMainLoop();
}