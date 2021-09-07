#include "PongMode.hpp"

//for the GL_ERRORS() macro:
#include "gl_errors.hpp"

//for glm::value_ptr() :
#include <glm/gtc/type_ptr.hpp>

#include <random>
#include <math.h>

PongMode::PongMode() {

	//set up trail as if ball has been here for 'forever':
	ball_trail.clear();
	ball_trail.emplace_back(ball, trail_length);
	ball_trail.emplace_back(ball, 0.0f);

	
	//----- allocate OpenGL resources -----
	{ //vertex buffer:
		glGenBuffers(1, &vertex_buffer);
		//for now, buffer will be un-filled.

		GL_ERRORS(); //PARANOIA: print out any OpenGL errors that may have happened
	}

	{ //vertex array mapping buffer for color_texture_program:
		//ask OpenGL to fill vertex_buffer_for_color_texture_program with the name of an unused vertex array object:
		glGenVertexArrays(1, &vertex_buffer_for_color_texture_program);

		//set vertex_buffer_for_color_texture_program as the current vertex array object:
		glBindVertexArray(vertex_buffer_for_color_texture_program);

		//set vertex_buffer as the source of glVertexAttribPointer() commands:
		glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);

		//set up the vertex array object to describe arrays of PongMode::Vertex:
		glVertexAttribPointer(
			color_texture_program.Position_vec4, //attribute
			3, //size
			GL_FLOAT, //type
			GL_FALSE, //normalized
			sizeof(Vertex), //stride
			(GLbyte *)0 + 0 //offset
		);
		glEnableVertexAttribArray(color_texture_program.Position_vec4);
		//[Note that it is okay to bind a vec3 input to a vec4 attribute -- the w component will be filled with 1.0 automatically]

		glVertexAttribPointer(
			color_texture_program.Color_vec4, //attribute
			4, //size
			GL_UNSIGNED_BYTE, //type
			GL_TRUE, //normalized
			sizeof(Vertex), //stride
			(GLbyte *)0 + 4*3 //offset
		);
		glEnableVertexAttribArray(color_texture_program.Color_vec4);

		glVertexAttribPointer(
			color_texture_program.TexCoord_vec2, //attribute
			2, //size
			GL_FLOAT, //type
			GL_FALSE, //normalized
			sizeof(Vertex), //stride
			(GLbyte *)0 + 4*3 + 4*1 //offset
		);
		glEnableVertexAttribArray(color_texture_program.TexCoord_vec2);

		//done referring to vertex_buffer, so unbind it:
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		//done setting up vertex array object, so unbind it:
		glBindVertexArray(0);

		GL_ERRORS(); //PARANOIA: print out any OpenGL errors that may have happened
	}

	{ //solid white texture:
		//ask OpenGL to fill white_tex with the name of an unused texture object:
		glGenTextures(1, &white_tex);

		//bind that texture object as a GL_TEXTURE_2D-type texture:
		glBindTexture(GL_TEXTURE_2D, white_tex);

		//upload a 1x1 image of solid white to the texture:
		glm::uvec2 size = glm::uvec2(1,1);
		std::vector< glm::u8vec4 > data(size.x*size.y, glm::u8vec4(0xff, 0xff, 0xff, 0xff));
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size.x, size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, data.data());

		//set filtering and wrapping parameters:
		//(it's a bit silly to mipmap a 1x1 texture, but I'm doing it because you may want to use this code to load different sizes of texture)
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

		//since texture uses a mipmap and we haven't uploaded one, instruct opengl to make one for us:
		glGenerateMipmap(GL_TEXTURE_2D);

		//Okay, texture uploaded, can unbind it:
		glBindTexture(GL_TEXTURE_2D, 0);

		GL_ERRORS(); //PARANOIA: print out any OpenGL errors that may have happened
	}
}

PongMode::~PongMode() {

	//----- free OpenGL resources -----
	glDeleteBuffers(1, &vertex_buffer);
	vertex_buffer = 0;

	glDeleteVertexArrays(1, &vertex_buffer_for_color_texture_program);
	vertex_buffer_for_color_texture_program = 0;

	glDeleteTextures(1, &white_tex);
	white_tex = 0;
}

bool PongMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {

	if (evt.type == SDL_KEYDOWN) {
		if (evt.key.keysym.sym == SDLK_a || evt.key.keysym.sym == SDLK_LEFT) {
			left_pressed = true;
			// ball_angle += 0.2f;
			// ball_angle = ball_angle < -0.5f ? ball_angle : -0.5f;
		}
		if (evt.key.keysym.sym == SDLK_d || evt.key.keysym.sym == SDLK_RIGHT) {
			right_pressed = true;
			// ball_angle -= 0.2f;
			// ball_angle = ball_angle > -PI + 0.5f ? ball_angle : -PI + 0.5f;
		}
		if (evt.key.keysym.sym == SDLK_SPACE) {
			// is_charging = true;
			space_pressed = true;
		}
	}

	if (evt.type == SDL_KEYUP) {
		if (evt.key.keysym.sym == SDLK_a || evt.key.keysym.sym == SDLK_LEFT) {
			left_pressed = false;
		}
		if (evt.key.keysym.sym == SDLK_d || evt.key.keysym.sym == SDLK_RIGHT) {
			right_pressed = false;
		}
		if (evt.key.keysym.sym == SDLK_SPACE) {
			ball_velocity.x = cos(ball_angle) * meter_fill * shot_power;
			ball_velocity.y = sin(ball_angle) * meter_fill * shot_power;
			meter_fill = 0.0f;
			// is_charging = false;
			is_charging = true;
			space_pressed = false;
		}
	}

	return false;
}

void PongMode::update(float elapsed) {

	//----- ball update -----

	//speed of ball doubles every four points:
	float speed_multiplier = 4.0f * std::pow(2.0f, (left_score + right_score) / 4.0f);

	//velocity cap, though (otherwise ball can pass through paddles):
	speed_multiplier = std::min(speed_multiplier, 10.0f);

	ball_velocity.x += ball_velocity.x > 0 ? -drag * elapsed : drag * elapsed;
	ball_velocity.y += elapsed * gravity;
	ball += elapsed * speed_multiplier * ball_velocity;

	//change ball angle:
	if (left_pressed) {
		ball_angle += turn_rate * elapsed;
		ball_angle = ball_angle < -0.5f ? ball_angle : -0.5f;
	}
	if (right_pressed) {
		ball_angle -= turn_rate * elapsed;
		ball_angle = ball_angle > -PI + 0.5f ? ball_angle : -PI + 0.5f;
	}

	//strength meter:
	if (space_pressed) {
		meter_fill += is_charging ? elapsed * meter_rate : elapsed * -meter_rate;
		if (meter_fill >= 1.0f) {
			meter_fill = 1.0f;
			is_charging = false;
		} else if (meter_fill <= 0.0f) {
			meter_fill = 0.0f;
			is_charging = true;
		}
	}

	//---- collision handling ----
	static std::mt19937 mt; //mersenne twister pseudo-random number generator

	//paddles:
	auto flag_vs_ball = [this]() {
		//compute area of overlap:
		glm::vec2 min = glm::max(flag - flag_radius, ball - ball_radius);
		glm::vec2 max = glm::min(flag + flag_radius, ball + ball_radius);

		//if no overlap, no collision:
		if (min.x > max.x || min.y > max.y) return;
		flag.x = (mt() / float(mt.max())) * 2 * court_radius.x - court_radius.x;
		flag.y = (mt() / float(mt.max())) * 2 * court_radius.y - court_radius.y;
		left_score++;
	};
	flag_vs_ball();

	//court walls:
	if (ball.y > court_radius.y - ball_radius.y) {
		ball.y = court_radius.y - ball_radius.y;
		if (ball_velocity.y > 0.0f) {
			ball_velocity.y = -ball_velocity.y;
		}
	}
	if (ball.y < -court_radius.y + ball_radius.y) {
		ball.y = -court_radius.y + ball_radius.y;
		if (ball_velocity.y < 0.0f) {
			ball_velocity.y = -ball_velocity.y - bounce_constant;
		}
		// if (ball_velocity.x > 0) {
		// 	ball_velocity.x -= bounce_constant;
		// } else {
		// 	ball_velocity.x += bounce_constant;
		// }
		if (abs(ball_velocity.x) < bounce_constant) {
			ball_velocity.x = 0.0f;
			ball_velocity.y = 0.0f;
		}
		// if (abs(ball_velocity.y) < bounce_constant) {
		// 	ball_velocity.y = 0.0f;
		// }
	}

	if (ball.x > court_radius.x - ball_radius.x) {
		ball.x = court_radius.x - ball_radius.x;
		if (ball_velocity.x > 0.0f) {
			ball_velocity.x = -ball_velocity.x;
			ball_velocity.x += bounce_constant;
		}
	}
	if (ball.x < -court_radius.x + ball_radius.x) {
		ball.x = -court_radius.x + ball_radius.x;
		if (ball_velocity.x < 0.0f) {
			ball_velocity.x = -ball_velocity.x;
			ball_velocity.x -= bounce_constant;
		}
	}

	//----- gradient trails -----

	//age up all locations in ball trail:
	for (auto &t : ball_trail) {
		t.z += elapsed;
	}
	//store fresh location at back of ball trail:
	ball_trail.emplace_back(ball, 0.0f);

	//trim any too-old locations from back of trail:
	//NOTE: since trail drawing interpolates between points, only removes back element if second-to-back element is too old:
	while (ball_trail.size() >= 2 && ball_trail[1].z > trail_length) {
		ball_trail.pop_front();
	}
}

void PongMode::draw(glm::uvec2 const &drawable_size) {
	//some nice colors from the course web page:
	#define HEX_TO_U8VEC4( HX ) (glm::u8vec4( (HX >> 24) & 0xff, (HX >> 16) & 0xff, (HX >> 8) & 0xff, (HX) & 0xff ))
	const glm::u8vec4 bg_color = HEX_TO_U8VEC4(0x193b59ff);
	const glm::u8vec4 fg_color = HEX_TO_U8VEC4(0xf2d2b6ff);
	const glm::u8vec4 shadow_color = HEX_TO_U8VEC4(0xf2ad94ff);
	const std::vector< glm::u8vec4 > trail_colors = {
		HEX_TO_U8VEC4(0xf2ad9488),
		HEX_TO_U8VEC4(0xf2897288),
		HEX_TO_U8VEC4(0xbacac088),
	};
	#undef HEX_TO_U8VEC4

	//other useful drawing constants:
	const float wall_radius = 0.05f;
	const float shadow_offset = 0.07f;
	const float padding = 0.14f; //padding between outside of walls and edge of window

	//---- compute vertices to draw ----

	//vertices will be accumulated into this list and then uploaded+drawn at the end of this function:
	std::vector< Vertex > vertices;

	//inline helper function for rectangle drawing:
	auto draw_rectangle = [&vertices](glm::vec2 const &center, glm::vec2 const &radius, glm::u8vec4 const &color) {
		//draw rectangle as two CCW-oriented triangles:
		vertices.emplace_back(glm::vec3(center.x-radius.x, center.y-radius.y, 0.0f), color, glm::vec2(0.5f, 0.5f));
		vertices.emplace_back(glm::vec3(center.x+radius.x, center.y-radius.y, 0.0f), color, glm::vec2(0.5f, 0.5f));
		vertices.emplace_back(glm::vec3(center.x+radius.x, center.y+radius.y, 0.0f), color, glm::vec2(0.5f, 0.5f));

		vertices.emplace_back(glm::vec3(center.x-radius.x, center.y-radius.y, 0.0f), color, glm::vec2(0.5f, 0.5f));
		vertices.emplace_back(glm::vec3(center.x+radius.x, center.y+radius.y, 0.0f), color, glm::vec2(0.5f, 0.5f));
		vertices.emplace_back(glm::vec3(center.x-radius.x, center.y+radius.y, 0.0f), color, glm::vec2(0.5f, 0.5f));
	};

	//inline helper function for circle drawing:
	auto draw_circle = [&vertices](glm::vec2 const &center, float const &radius, glm::u8vec4 const &color) {
		//draw ball as fan of triangles
		float increment = 0.05f;
		float angle = 0.0f;
		float PI = 3.1416f;
		while(angle < 2 * PI) {
			vertices.emplace_back(glm::vec3(center.x, center.y, 0.0f), color, glm::vec2(0.5f, 0.5f));
			vertices.emplace_back(glm::vec3(center.x-radius*cos(angle), center.y-radius*sin(angle), 0.0f), color, glm::vec2(0.5f, 0.5f));
			vertices.emplace_back(glm::vec3(center.x-radius*cos(angle+increment), center.y-radius*sin(angle+increment), 0.0f), color, glm::vec2(0.5f, 0.5f));
			angle += increment; 
		}
	};


	//ball's trail:
	if (ball_trail.size() >= 2) {
		//start ti at second element so there is always something before it to interpolate from:
		std::deque< glm::vec3 >::iterator ti = ball_trail.begin() + 1;
		//draw trail from oldest-to-newest:
		constexpr uint32_t STEPS = 20;
		//draw from [STEPS, ..., 1]:
		for (uint32_t step = STEPS; step > 0; --step) {
			//time at which to draw the trail element:
			float t = step / float(STEPS) * trail_length;
			//advance ti until 'just before' t:
			while (ti != ball_trail.end() && ti->z > t) ++ti;
			//if we ran out of recorded tail, stop drawing:
			if (ti == ball_trail.end()) break;
			//interpolate between previous and current trail point to the correct time:
			glm::vec3 a = *(ti-1);
			glm::vec3 b = *(ti);
			glm::vec2 at = (t - a.z) / (b.z - a.z) * (glm::vec2(b) - glm::vec2(a)) + glm::vec2(a);

			//look up color using linear interpolation:
			//compute (continuous) index:
			float c = (step-1) / float(STEPS-1) * trail_colors.size();
			//split into an integer and fractional portion:
			int32_t ci = int32_t(std::floor(c));
			float cf = c - ci;
			//clamp to allowable range (shouldn't ever be needed but good to think about for general interpolation):
			if (ci < 0) {
				ci = 0;
				cf = 0.0f;
			}
			if (ci > int32_t(trail_colors.size())-2) {
				ci = int32_t(trail_colors.size())-2;
				cf = 1.0f;
			}
			//do the interpolation (casting to floating point vectors because glm::mix doesn't have an overload for u8 vectors):
			glm::u8vec4 color = glm::u8vec4(
				glm::mix(glm::vec4(trail_colors[ci]), glm::vec4(trail_colors[ci+1]), cf)
			);

			//draw:
			// draw_rectangle(at, ball_radius * ((trail_length - b.z) / trail_length), color);
			draw_circle(at, ball_radius.x * ((trail_length - b.z) / trail_length), color);
		}
	}

	//solid objects:

	//walls:
	draw_rectangle(glm::vec2(-court_radius.x-wall_radius, 0.0f), glm::vec2(wall_radius, court_radius.y + 2.0f * wall_radius), fg_color);
	draw_rectangle(glm::vec2( court_radius.x+wall_radius, 0.0f), glm::vec2(wall_radius, court_radius.y + 2.0f * wall_radius), fg_color);
	draw_rectangle(glm::vec2( 0.0f,-court_radius.y-wall_radius), glm::vec2(court_radius.x, wall_radius), fg_color);
	draw_rectangle(glm::vec2( 0.0f, court_radius.y+wall_radius), glm::vec2(court_radius.x, wall_radius), fg_color);

	//paddles:
	draw_rectangle(meter, meter_size, fg_color);
	// draw_rectangle(right_paddle, paddle_radius, fg_color);
	

	//ball:
	draw_circle(ball, ball_radius.x, fg_color);

	//some convenient constants:
	glm::vec2 plus = glm::vec2(ball_radius.x, 0.1);
	glm::vec2 minus = -1.0f * plus;
	float s = sin(ball_angle);
	float c = cos(ball_angle);

	//draw rectangle to show the angle of aim:
	glm::vec2 nozzle_center = glm::vec2(ball.x - 0.5 * c, ball.y - 0.5 * s);
	vertices.emplace_back(glm::vec3(nozzle_center.x + (minus.x) * c - (minus.y) * s, nozzle_center.y + (minus.x) * s + (minus.y) * c, 0.0f), shadow_color, glm::vec2(0.5f, 0.5f));
	vertices.emplace_back(glm::vec3(nozzle_center.x + (plus.x) * c - (minus.y) * s, nozzle_center.y + (plus.x) * s + (minus.y) * c, 0.0f), shadow_color, glm::vec2(0.5f, 0.5f));
	vertices.emplace_back(glm::vec3(nozzle_center.x + (plus.x) * c - (plus.y) * s, nozzle_center.y + (plus.x) * s + (plus.y) * c, 0.0f), shadow_color, glm::vec2(0.5f, 0.5f));

	vertices.emplace_back(glm::vec3(nozzle_center.x + (minus.x) * c - (minus.y) * s, nozzle_center.y + (minus.x) * s + (minus.y) * c, 0.0f), shadow_color, glm::vec2(0.5f, 0.5f));
	vertices.emplace_back(glm::vec3(nozzle_center.x + (plus.x) * c - (plus.y) * s, nozzle_center.y + (plus.x) * s + (plus.y) * c, 0.0f), shadow_color, glm::vec2(0.5f, 0.5f));
	vertices.emplace_back(glm::vec3(nozzle_center.x + (minus.x) * c - (plus.y) * s, nozzle_center.y + (minus.x) * s + (plus.y) * c, 0.0f), shadow_color, glm::vec2(0.5f, 0.5f));

	//scores:
	glm::vec2 score_radius = glm::vec2(0.1f, 0.1f);
	for (uint32_t i = 0; i < left_score; ++i) {
		draw_rectangle(glm::vec2( -court_radius.x + (2.0f + 3.0f * i) * score_radius.x, court_radius.y + 2.0f * wall_radius + 2.0f * score_radius.y), score_radius, fg_color);
	}
	for (uint32_t i = 0; i < right_score; ++i) {
		draw_rectangle(glm::vec2( court_radius.x - (2.0f + 3.0f * i) * score_radius.x, court_radius.y + 2.0f * wall_radius + 2.0f * score_radius.y), score_radius, fg_color);
	}

	//strength meter:
	draw_rectangle(glm::vec2(meter.x, meter.y - meter_size.y + (meter_size.y * meter_fill)), glm::vec2(meter_size.x, meter_size.y * meter_fill), shadow_color);

	//flag:
	vertices.emplace_back(glm::vec3(flag.x + flag_radius.x, flag.y - flag_radius.y, 0.0f), fg_color, glm::vec2(0.5f,0.5f));
	vertices.emplace_back(glm::vec3(flag.x - flag_radius.x, flag.y - flag_radius.y, 0.0f), fg_color, glm::vec2(0.5f,0.5f));
	vertices.emplace_back(glm::vec3(flag.x, flag.y + flag_radius.y, 0.0f), fg_color, glm::vec2(0.5f,0.5f));


	//------ compute court-to-window transform ------

	//compute area that should be visible:
	glm::vec2 scene_min = glm::vec2(
		-court_radius.x - 2.0f * wall_radius - padding,
		-court_radius.y - 2.0f * wall_radius - padding
	);
	glm::vec2 scene_max = glm::vec2(
		court_radius.x + 2.0f * wall_radius + padding,
		court_radius.y + 2.0f * wall_radius + 3.0f * score_radius.y + padding
	);

	//compute window aspect ratio:
	float aspect = drawable_size.x / float(drawable_size.y);
	//we'll scale the x coordinate by 1.0 / aspect to make sure things stay square.

	//compute scale factor for court given that...
	float scale = std::min(
		(2.0f * aspect) / (scene_max.x - scene_min.x), //... x must fit in [-aspect,aspect] ...
		(2.0f) / (scene_max.y - scene_min.y) //... y must fit in [-1,1].
	);

	glm::vec2 center = 0.5f * (scene_max + scene_min);

	//build matrix that scales and translates appropriately:
	glm::mat4 court_to_clip = glm::mat4(
		glm::vec4(scale / aspect, 0.0f, 0.0f, 0.0f),
		glm::vec4(0.0f, scale, 0.0f, 0.0f),
		glm::vec4(0.0f, 0.0f, 1.0f, 0.0f),
		glm::vec4(-center.x * (scale / aspect), -center.y * scale, 0.0f, 1.0f)
	);
	//NOTE: glm matrices are specified in *Column-Major* order,
	// so each line above is specifying a *column* of the matrix(!)

	//also build the matrix that takes clip coordinates to court coordinates (used for mouse handling):
	clip_to_court = glm::mat3x2(
		glm::vec2(aspect / scale, 0.0f),
		glm::vec2(0.0f, 1.0f / scale),
		glm::vec2(center.x, center.y)
	);

	//---- actual drawing ----

	//clear the color buffer:
	glClearColor(bg_color.r / 255.0f, bg_color.g / 255.0f, bg_color.b / 255.0f, bg_color.a / 255.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	//use alpha blending:
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	//don't use the depth test:
	glDisable(GL_DEPTH_TEST);

	//upload vertices to vertex_buffer:
	glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer); //set vertex_buffer as current
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(vertices[0]), vertices.data(), GL_STREAM_DRAW); //upload vertices array
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	//set color_texture_program as current program:
	glUseProgram(color_texture_program.program);

	//upload OBJECT_TO_CLIP to the proper uniform location:
	glUniformMatrix4fv(color_texture_program.OBJECT_TO_CLIP_mat4, 1, GL_FALSE, glm::value_ptr(court_to_clip));

	//use the mapping vertex_buffer_for_color_texture_program to fetch vertex data:
	glBindVertexArray(vertex_buffer_for_color_texture_program);

	//bind the solid white texture to location zero so things will be drawn just with their colors:
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, white_tex);

	//run the OpenGL pipeline:
	glDrawArrays(GL_TRIANGLES, 0, GLsizei(vertices.size()));

	//unbind the solid white texture:
	glBindTexture(GL_TEXTURE_2D, 0);

	//reset vertex array to none:
	glBindVertexArray(0);

	//reset current program to none:
	glUseProgram(0);
	

	GL_ERRORS(); //PARANOIA: print errors just in case we did something wrong.

}
