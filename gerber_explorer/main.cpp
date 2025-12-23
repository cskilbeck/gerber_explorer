#include <cstdio>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "gerber_lib.h"
#include "gl_base.h"
#include "gl_colors.h"
#include "gl_drawer.h"
#include "gl_matrix.h"
#include "log_drawer.h"

using gerber_lib::gerber_error_code;

LOG_CONTEXT("main", info);

using namespace gerber_3d;
using namespace gerber_lib;
using namespace gerber_lib::gerber_2d;

void log_gl([[maybe_unused]] GLenum source, [[maybe_unused]] GLenum type, [[maybe_unused]] GLuint id, [[maybe_unused]] GLenum severity,
            [[maybe_unused]] GLsizei length, const GLchar *message, [[maybe_unused]] const void *userParam)
{
    LOG_INFO("{}", message);
}

void make_world_to_window_transform(gl_matrix result, rect const &window, rect const &view)
{
    gl_matrix scale;
    gl_matrix origin;

    make_scale(scale, (float)(window.width() / view.width()), (float)(window.height() / view.height()));
    make_translate(origin, -(float)view.min_pos.x, -(float)view.min_pos.y);
    matrix_multiply(scale, origin, result);
}

int flushed_puts(char const *s)
{
    int x = puts(s);
    fflush(stdout);
    return x;
}

// make boundary, keep for drawing outline
// tesselate

static int draw_call = 0;
static int draw_triangle = 0;

static void key(GLFWwindow* window, int key, [[maybe_unused]] int scancode, int action, [[maybe_unused]]int mods)
{
    if(action == GLFW_PRESS) {
        LOG_INFO("{}", key);
        switch(key) {
        case GLFW_KEY_ESCAPE:
            glfwSetWindowShouldClose(window, GL_TRUE);
            break;
        case GLFW_KEY_LEFT:
            draw_triangle -= 1;
            LOG_INFO("Triangle: {}", draw_triangle);
            break;
        case GLFW_KEY_RIGHT:
            draw_triangle += 1;
            LOG_INFO("Triangle: {}", draw_triangle);
            break;
        case GLFW_KEY_UP:
            draw_call += 1;
            LOG_INFO("Prim: {}", draw_call);
            break;
        case GLFW_KEY_DOWN:
            draw_call -= 1;
            LOG_INFO("Prim: {}", draw_call);
            break;
        }
    }
}

int main(int, char **)
{
    glfwInit();

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow *window = glfwCreateWindow(800, 600, "GLAD + GLFW", nullptr, nullptr);
    glfwSetKeyCallback(window, key);
    glfwMakeContextCurrent(window);

    if(!gladLoadGL()) {
        LOG_ERROR("GLAD LOAD FAILED, Exiting...");
        return 1;
    }

    log_set_level(log_level_debug);
    log_set_emitter_function(flushed_puts);

    // GL_CHECK(glDebugMessageCallback(log_gl, nullptr));
    // GL_CHECK(glEnable(GL_DEBUG_OUTPUT));

    // GL > 3.0 requires a vertex array even if it's not used...?
    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    gl_solid_program solid;
    solid.init();
    solid.set_color(0xff00ffff);

    gerber g;
    gl_drawer drawer;
    drawer.set_gerber(&g);
    drawer.program = &solid;
    g.parse_file("../../gerber_test_files/SMD_prim_20_X1.gbr");
    drawer.on_finished_loading();
    // log_drawer d;
    // d.set_gerber(&g);
    // g.draw(d);

    while(!glfwWindowShouldClose(window)) {

        glfwPollEvents();

        int window_width, window_height;
        glfwGetWindowSize(window, &window_width, &window_height);

        int fbWidth, fbHeight;
        glfwGetFramebufferSize(window, &fbWidth, &fbHeight);

        GL_CHECK(glViewport(0, 0, fbWidth, fbHeight));
        GL_CHECK(glClearColor(0.1f, 0.2f, 0.3f, 1.0f));
        GL_CHECK(glDisable(GL_DEPTH_TEST));
        GL_CHECK(glDisable(GL_CULL_FACE));

        rect window_rect{ { 0, 0 }, { static_cast<float>(window_width), static_cast<float>(window_height) } };
        rect view_rect{ { -8, -8 }, { static_cast<float>(window_width / 64.0f), static_cast<float>(window_height / 64.0f) } };

        gl_matrix projection_matrix_invert_y;
        make_ortho(projection_matrix_invert_y, window_width, -window_height);

        gl_matrix view_matrix;
        make_translate(view_matrix, 0, (float)-window_height);

        gl_matrix screen_matrix;
        matrix_multiply(projection_matrix_invert_y, view_matrix, screen_matrix);

        gl_matrix projection_matrix;
        make_ortho(projection_matrix, window_width, window_height);

        make_world_to_window_transform(view_matrix, window_rect, view_rect);

        gl_matrix world_transform_matrix;
        matrix_multiply(projection_matrix, view_matrix, world_transform_matrix);
        GL_CHECK(glUniformMatrix4fv(solid.transform_location, 1, true, world_transform_matrix));

        GL_CHECK(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
        GL_CHECK(glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, (void *)12));

        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

        drawer.render(gl_color::magenta);

        glfwSwapBuffers(window);
    }
    glfwTerminate();
}
