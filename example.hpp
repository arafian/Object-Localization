// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015 Intel Corporation. All Rights Reserved.

#pragma once

#define NOMINMAX
#include <Windows.h>
#include <stddef.h>
#include <GL/gl.h>
#include <GL/glu.h>

#include <string>
#include <sstream>
#include <iostream>

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <gl/gl.h>
#include <gl/glu.h>

#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "glu32.lib")

//////////////////////////////
// Basic Data Types         //
//////////////////////////////

struct float3 { float x, y, z; };
struct float2 { float x, y; };

struct rect
{
    float x, y;
    float w, h;

    // Create new rect within original boundaries with give aspect ration
    rect adjust_ratio(float2 size) const
    {
        auto H = static_cast<float>(h), W = static_cast<float>(h) * size.x / size.y;
        if (W > w)
        {
            auto scale = w / W;
            W *= scale;
            H *= scale;
        }

        return{ x + (w - W) / 2, y + (h - H) / 2, W, H };
    }
};

//////////////////////////////
// Simple font loading code //
//////////////////////////////

#include "stb_easy_font.h"

inline void draw_text(int x, int y, const char * text)
{
    char buffer[60000]; // ~300 chars
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(2, GL_FLOAT, 16, buffer);
    glDrawArrays(GL_QUADS, 0, 4 * stb_easy_font_print((float)x, (float)(y - 7), (char *)text, nullptr, buffer, sizeof(buffer)));
    glDisableClientState(GL_VERTEX_ARRAY);
}

////////////////////////
// Image display code //
////////////////////////

class texture
{
public:
    void render(const rs2::video_frame& frame, const rect& r)
    {
        upload(frame);
        show(r.adjust_ratio({ float(width), float(height) }));
    }

    void upload(const rs2::video_frame& frame)
    {
        if (!frame) return;

        if (!gl_handle)
            glGenTextures(1, &gl_handle);
        GLenum err = glGetError();

        auto format = frame.get_profile().format();
        width = frame.get_width();
        height = frame.get_height();
        stream = frame.get_profile().stream_type();

        glBindTexture(GL_TEXTURE_2D, gl_handle);

        switch (format)
        {
        case RS2_FORMAT_RGB8:
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, frame.get_data());
            break;
        case RS2_FORMAT_RGBA8:
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, frame.get_data());
            break;
        default:
            throw std::runtime_error("The requested format is not suported by this demo!");
        }

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    GLuint get_gl_handle() { return gl_handle; }

    void show(const rect& r) const
    {
        if (!gl_handle) return;

        glBindTexture(GL_TEXTURE_2D, gl_handle);
        glEnable(GL_TEXTURE_2D);
        glBegin(GL_QUAD_STRIP);
        glTexCoord2f(0.f, 1.f); glVertex2f(r.x, r.y + r.h);
        glTexCoord2f(0.f, 0.f); glVertex2f(r.x, r.y);
        glTexCoord2f(1.f, 1.f); glVertex2f(r.x + r.w, r.y + r.h);
        glTexCoord2f(1.f, 0.f); glVertex2f(r.x + r.w, r.y);
        glEnd();
        glDisable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);

        draw_text(r.x + 15, r.y + 20, rs2_stream_to_string(stream));
    }
private:
    GLuint gl_handle = 0;
    int width = 0;
    int height = 0;
    rs2_stream stream = RS2_STREAM_ANY;
};

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message)
    {
    break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
        break;
    }

    return DefWindowProc(hwnd, message, wparam, lparam);
}

class window
{
public:
    std::function<void(bool)>           on_left_mouse   = [](bool) {};
    std::function<void(double, double)> on_mouse_scroll = [](double, double) {};
    std::function<void(double, double)> on_mouse_move   = [](double, double) {};
    std::function<void(int)>            on_key_release  = [](int) {};

    window(int width, int height, const char* title)
        : _width(width), _height(height)
    {
        WNDCLASS wc;
        wc.cbClsExtra = 0;
        wc.cbWndExtra = 0;
        wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
        wc.hInstance = nullptr;
        wc.lpfnWndProc = WndProc;
        wc.lpszClassName = TEXT("Example");
        wc.lpszMenuName = 0;
        wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
        RegisterClass(&wc);

        _hwnd = CreateWindow(TEXT("Example"),
            title,
            WS_OVERLAPPEDWINDOW,
            GetSystemMetrics(SM_CXSCREEN) / 2 - _width / 2, 
            GetSystemMetrics(SM_CYSCREEN) / 2 - _height / 2,
            _width, _height,
            NULL, NULL,
            NULL, NULL);
        if (!_hwnd) throw;

        ShowWindow(_hwnd, 1);

        _hdc = GetDC(_hwnd);

        PIXELFORMATDESCRIPTOR pfd = { 0 };
        pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
        pfd.nVersion = 1;

        pfd.dwFlags = PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER | PFD_DRAW_TO_WINDOW;
        pfd.iPixelType = PFD_TYPE_RGBA;
        pfd.cColorBits = 24;
        pfd.cDepthBits = 32;

        int chosenPixelFormat = ChoosePixelFormat(_hdc, &pfd);
        if (chosenPixelFormat == 0) throw;

        if (!SetPixelFormat(_hdc, chosenPixelFormat, &pfd)) throw;

        _hglrc = wglCreateContext(_hdc);
        wglMakeCurrent(_hdc, _hglrc);
    }

    float width() const { return float(_width); }
    float height() const { return float(_height); }

    operator bool()
    {
        glPopMatrix();
        SwapBuffers(_hdc);

        MSG msg;
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT) return false;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        RECT rect;
        if (GetWindowRect(_hwnd, &rect))
        {
            _width = rect.right - rect.left;
            _height = rect.bottom - rect.top;
        }

        POINT cursor;
        GetCursorPos(&cursor);
        if (cursor.x != _cursor.x || cursor.y != _cursor.y)
        {
            on_mouse_move(cursor.x, cursor.y);
            _cursor = cursor;
        }
        on_left_mouse((GetKeyState(VK_LBUTTON) & 0x100) != 0);

        // Clear the framebuffer
        glClear(GL_COLOR_BUFFER_BIT);
        glViewport(0, 0, _width, _height);

        // Draw the images
        glPushMatrix();
        glOrtho(0, _width, _height, 0, -1, +1);

        return true;
    }

    ~window()
    {
        wglMakeCurrent(NULL, NULL);
        wglDeleteContext(_hglrc);
        ReleaseDC(_hwnd, _hdc);
    }
private:
    int _width, _height;

    HWND  _hwnd;
    HDC   _hdc;
    HGLRC _hglrc;
    POINT _cursor;
};


// Struct for managing rotation of pointcloud view
struct glfw_state {
    glfw_state() : yaw(15.0), pitch(15.0), last_x(0.0), last_y(0.0),
        ml(false), offset_x(2.f), offset_y(2.f), tex() {}
    double yaw;
    double pitch;
    double last_x;
    double last_y;
    bool ml;
    float offset_x;
    float offset_y;
    texture tex;
};


// Handles all the OpenGL calls needed to display the point cloud
void draw_pointcloud(window& app, glfw_state& app_state, rs2::points& points)
{
    if (!points)
        return;

    // OpenGL commands that prep screen for the pointcloud
    glPopMatrix();
    glPushAttrib(GL_ALL_ATTRIB_BITS);

    float width = app.width(), height = app.height();

    glClearColor(153.f / 255, 153.f / 255, 153.f / 255, 1);
    glClear(GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    gluPerspective(60, width / height, 0.01f, 10.0f);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    gluLookAt(0, 0, 0, 0, 0, 1, 0, -1, 0);

    glTranslatef(0, 0, +0.5f + app_state.offset_y*0.05f);
    glRotated(app_state.pitch, 1, 0, 0);
    glRotated(app_state.yaw, 0, 1, 0);
    glTranslatef(0, 0, -0.5f);

    glPointSize(width / 640);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, app_state.tex.get_gl_handle());
    float tex_border_color[] = { 0.8f, 0.8f, 0.8f, 0.8f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, tex_border_color);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, 0x812F); // GL_CLAMP_TO_EDGE
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, 0x812F); // GL_CLAMP_TO_EDGE
    glBegin(GL_POINTS);


    /* this segment actually prints the pointcloud */
    auto vertices = points.get_vertices();              // get vertices
    auto tex_coords = points.get_texture_coordinates(); // and texture coordinates
    for (int i = 0; i < points.size(); i++)
    {
        if (vertices[i].z)
        {
            // upload the point and texture coordinates only for points we have depth data for
            glVertex3fv(vertices[i]);
            glTexCoord2fv(tex_coords[i]);
        }
    }

    // OpenGL cleanup
    glEnd();
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glPopAttrib();
    glPushMatrix();
}

// Registers the state variable and callbacks to allow mouse control of the pointcloud
void register_glfw_callbacks(window& app, glfw_state& app_state)
{
    app.on_left_mouse = [&](bool pressed)
    {
        app_state.ml = pressed;
    };

    app.on_mouse_scroll = [&](double xoffset, double yoffset)
    {
        app_state.offset_x -= static_cast<float>(xoffset);
        app_state.offset_y -= static_cast<float>(yoffset);
    };

    app.on_mouse_move = [&](double x, double y)
    {
        if (app_state.ml)
        {
            app_state.yaw -= (x - app_state.last_x);
            app_state.yaw = std::max(app_state.yaw, -120.0);
            app_state.yaw = std::min(app_state.yaw, +120.0);
            app_state.pitch += (y - app_state.last_y);
            app_state.pitch = std::max(app_state.pitch, -80.0);
            app_state.pitch = std::min(app_state.pitch, +80.0);
        }
        app_state.last_x = x;
        app_state.last_y = y;
    };

    app.on_key_release = [&](int key)
    {
        if (key == 32) // Escape
        {
            app_state.yaw = app_state.pitch = 0; app_state.offset_x = app_state.offset_y = 0.0;
        }
    };
}
