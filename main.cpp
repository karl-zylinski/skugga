#include <windows.h>
#include <time.h>
#include "object.h"
#include "types.h"
#include "helpers.h"

// This must be last of the includes!
#include "build_include.h"

void key_pressed_callback(Key key)
{
    keyboard_state.pressed[(unsigned)key] = true;
    keyboard_state.held[(unsigned)key] = true;
}

void key_released_callback(Key key)
{
    keyboard_state.released[(unsigned)key] = true;
    keyboard_state.held[(unsigned)key] = false;
}

void mouse_moved_callback(const Vector2i& delta)
{
    mouse_state.delta += delta;
}

int main()
{
    unsigned temp_memory_size = 1024 * 1024 * 1024;
    void* temp_memory_block = VirtualAlloc(nullptr, temp_memory_size, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
    temp_memory::init(temp_memory_block, temp_memory_size);
    srand((unsigned)time(0));
    Window window = {0};
    window::init(&window);
    memset(&keyboard_state, 0, sizeof(Keyboard));
    memset(&mouse_state, 0, sizeof(Mouse));
    window.key_released_callback = key_released_callback;
    window.key_pressed_callback = key_pressed_callback;
    window.mouse_moved_callback = mouse_moved_callback;
    RendererState renderer_state = {0};
    SimulationState simulation_state = {0};
    renderer::init(&renderer_state, window.handle);
    RenderTarget back_buffer = renderer::create_back_buffer(&renderer_state);
    RenderTarget render_texture = renderer::create_render_texture(&renderer_state, PixelFormat::R8G8B8A8_UINT_NORM);
    renderer::set_render_target(&renderer_state, &back_buffer);
    simulation::init(&simulation_state, &renderer_state);
    camera::init(&simulation_state.camera);
    Color clear_color = {0, 0, 0, 1};

    while(!window.closed)
    {
        window::process_all_messsages();
        simulation::simulate(&simulation_state);
        renderer::clear_depth_stencil(&renderer_state);
        renderer::clear_render_target(&renderer_state, &back_buffer, clear_color);

        for (unsigned i = 0; i < simulation_state.world.num_objects; ++i)
        {
            if (simulation_state.world.objects[i].valid)
                renderer::draw(&renderer_state, simulation_state.world.objects[i].geometry_handle, simulation_state.world.objects[i].world_transform, simulation_state.camera.view_matrix, simulation_state.camera.projection_matrix);
        }

        keyboard::end_of_frame();
        mouse::end_of_frame();
        renderer::present(&renderer_state);
    }

    /*{
        Allocator ta = create_temp_allocator();
        Image i = renderer::read_back_texture(&ta, &renderer_state, render_texture);
        File out = {};
        out.data = i.data;
        out.size = image::calc_size(i.pixel_format, i.width, i.height);
        file::write(out, "texture");
    }*/

    renderer::shutdown(&renderer_state);
    return 0;
}