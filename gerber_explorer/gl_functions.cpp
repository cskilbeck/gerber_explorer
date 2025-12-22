#include <stdio.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <gl/GL.h>

#include "Wglext.h"
#include "glcorearb.h"

#include "gl_functions.h"

//////////////////////////////////////////////////////////////////////

namespace
{
    typedef PROC(__stdcall *GL3WglGetProcAddr)(LPCSTR);
    static GL3WglGetProcAddr wgl_get_proc_address{ nullptr };

    template <typename T> void get_proc(char const *function_name, T &function_pointer)
    {
        if(wgl_get_proc_address == nullptr) {
            HMODULE gl_dll = LoadLibraryA("opengl32.dll");
            if(gl_dll == nullptr) {
                fprintf(stderr, "ERROR: Can't load OpenGL32.dll\n");
                ExitProcess(0);
            }
            wgl_get_proc_address = (GL3WglGetProcAddr)GetProcAddress(gl_dll, "wglGetProcAddress");
            if(wgl_get_proc_address == nullptr) {
                fprintf(stderr, "ERROR: Can't get proc address for wglGetProcAddress\n");
                ExitProcess(0);
            }
        }
        function_pointer = reinterpret_cast<T>(wgl_get_proc_address(function_name));
        if(function_pointer == nullptr) {
            fprintf(stderr, "ERROR: Can't get proc address for %s\n", function_name);
            ExitProcess(1);
        }
    }

}    // namespace

#define GET_PROC(x) get_proc(#x, x)

void init_gl_functions()
{
#undef GL_FUNCTION
#define GL_FUNCTION(fn_type, fn_name) GET_PROC(fn_name)
#include "gl_function_list.h"
#undef GL_FUNCTION
}

#undef GL_FUNCTION
#define GL_FUNCTION(fn_type, fn_name) fn_type fn_name
#include "gl_function_list.h"
#undef GL_FUNCTION
