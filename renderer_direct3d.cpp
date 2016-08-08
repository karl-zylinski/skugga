#include "renderer_direct3d.h"
#include <D3Dcompiler.h>
#include "world.h"
#include "config.h"
#include "rect.h"
#include "file.h"

namespace
{
    DXGI_FORMAT pixel_format_to_dxgi_format(PixelFormat pf)
    {
        switch(pf)
        {
        case PixelFormat::R8G8B8A8_UINT_NORM:
            return DXGI_FORMAT_R8G8B8A8_UNORM;
        case PixelFormat::R32G32B32A32_FLOAT:
            return DXGI_FORMAT_R32G32B32A32_FLOAT;
        default:
            return DXGI_FORMAT_UNKNOWN;
        }
    }

    void check_ok(HRESULT res)
    {
        if (res >= 0)
        {
            return;
        }

        static wchar_t msg[120];
        wsprintf(msg, L"Error in renderer: %0x", res);
        MessageBox(nullptr, msg, nullptr, 0);
    }
}

void Renderer::init(HWND window_handle)
{
    DXGI_SWAP_CHAIN_DESC scd = {};
    scd.BufferCount = 1;
    scd.BufferDesc.Width = WindowWidth;
    scd.BufferDesc.Height = WindowHeight;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = window_handle;
    scd.SampleDesc.Count = 1;
    scd.Windowed = true;

    check_ok(D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        0,
        0,
        0,
        D3D11_SDK_VERSION,
        &scd,
        &swap_chain,
        &device,
        nullptr,
        &device_context
    ));

    D3D11_VIEWPORT viewport = {};
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width = WindowWidth;
    viewport.Height = WindowHeight;
    viewport.MinDepth = 0;
    viewport.MaxDepth = 1;
    device_context->RSSetViewports(1, &viewport);
    D3D11_BUFFER_DESC cbd = {0};
    cbd.ByteWidth = sizeof(ConstantBuffer);
    cbd.Usage = D3D11_USAGE_DYNAMIC;
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    cbd.MiscFlags = 0;
    cbd.StructureByteStride = 0;
    device->CreateBuffer(&cbd, nullptr, &constant_buffer);
    D3D11_TEXTURE2D_DESC dstd = {0};
    dstd.Width = WindowWidth;
    dstd.Height = WindowHeight;
    dstd.MipLevels = 1;
    dstd.ArraySize = 1;
    dstd.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dstd.SampleDesc.Count = 1;
    dstd.Usage = D3D11_USAGE_DEFAULT;
    dstd.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    dstd.CPUAccessFlags = 0;
    dstd.MiscFlags = 0;
    device->CreateTexture2D(&dstd, nullptr, &depth_stencil_texture);
    D3D11_DEPTH_STENCIL_VIEW_DESC dsvd;
    dsvd.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsvd.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    dsvd.Texture2D.MipSlice = 0;
    dsvd.Flags = 0;
    device->CreateDepthStencilView(depth_stencil_texture, &dsvd, &depth_stencil_view);

    back_buffer = create_back_buffer();
    set_render_target(&back_buffer);

    D3D11_RASTERIZER_DESC rsd;
    rsd.FillMode = D3D11_FILL_SOLID;
    rsd.CullMode = D3D11_CULL_BACK;
    rsd.FrontCounterClockwise = false;
    rsd.DepthBias = false;
    rsd.DepthBiasClamp = 0;
    rsd.SlopeScaledDepthBias = 0;
    rsd.DepthClipEnable = true;
    rsd.ScissorEnable = true;
    rsd.MultisampleEnable = false;
    rsd.AntialiasedLineEnable = false;
    device->CreateRasterizerState(&rsd, &raster_state);
    device_context->RSSetState(raster_state);
    
    disable_scissor();
}

void Renderer::shutdown()
{
    for (unsigned i = 0; i < num_resources; ++ i)
    {
        if (resources[i].type != RenderResourceType::Unused)
            unload_resource({i});
    }

    depth_stencil_texture->Release();
    depth_stencil_view->Release();
    device->Release();
    device_context->Release();
}

RRHandle Renderer::load_shader(const wchar* filename)
{
    unsigned handle = find_free_resource_handle();

    if (handle == InvalidHandle)
        return {InvalidHandle};

    ID3DBlob* vs_blob;
    ID3DBlob* ps_blob;
    D3DCompileFromFile(filename, 0, 0, "VShader", "vs_4_0", 0, 0, &vs_blob, 0);
    D3DCompileFromFile(filename, 0, 0, "PShader", "ps_4_0", 0, 0, &ps_blob, 0);
    Shader s = {};
    device->CreateVertexShader(vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(), nullptr, &s.vertex_shader);
    device->CreatePixelShader(ps_blob->GetBufferPointer(), ps_blob->GetBufferSize(), nullptr, &s.pixel_shader);
    D3D11_INPUT_ELEMENT_DESC ied[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"LIGHT_EMITTANCE", 0, DXGI_FORMAT_R32_FLOAT, 0, 48, D3D11_INPUT_PER_VERTEX_DATA, 0}
    };
    device->CreateInputLayout(ied, 5, vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(), &s.input_layout);
    vs_blob->Release();
    ps_blob->Release();
    RenderResource r;
    r.type = RenderResourceType::Shader;
    r.shader = s;
    resources[handle] = r;
    return {handle};
}

void Renderer::set_shader(RRHandle shader)
{
    Shader& s = get_resource(shader).shader;
    device_context->VSSetShader(s.vertex_shader, 0, 0);
    device_context->PSSetShader(s.pixel_shader, 0, 0);
    device_context->IASetInputLayout(s.input_layout);
}

RenderTarget Renderer::create_back_buffer()
{
    unsigned handle = find_free_resource_handle();
    Assert(handle != InvalidHandle, "Couldn't create back-buffer.");

    RenderTargetResource rts = {};
    ID3D11Texture2D* back_buffer_texture;
    swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&back_buffer_texture);

    D3D11_TEXTURE2D_DESC td = {};
    back_buffer_texture->GetDesc(&td);

    device->CreateRenderTargetView(back_buffer_texture, nullptr, &rts.view);
    back_buffer_texture->Release();

    RenderResource r = {};
    r.type = RenderResourceType::RenderTarget;
    r.render_target = rts;
    resources[handle] = r;

    RenderTarget rt;
    rt.render_resource = {handle};
    rt.width = td.Width;
    rt.height = td.Height;
    rt.clear = true;
    rt.clear_depth_stencil = true;
    rt.clear_color = {0.2f, 0, 0, 1};
    return rt;
}

RenderTarget Renderer::create_render_texture(PixelFormat pf)
{
    unsigned handle = find_free_resource_handle();
    Assert(handle != InvalidHandle, "Couldn't create render texture.");

    DXGI_SWAP_CHAIN_DESC scd = {};
    swap_chain->GetDesc(&scd);
    D3D11_TEXTURE2D_DESC rtd = {};
    rtd.Width = scd.BufferDesc.Width;
    rtd.Height = scd.BufferDesc.Height;
    rtd.MipLevels = 1;
    rtd.ArraySize = 1;
    rtd.Format = pixel_format_to_dxgi_format(pf);
    rtd.SampleDesc.Count = 1;
    rtd.Usage = D3D11_USAGE_DEFAULT;
    rtd.BindFlags = D3D11_BIND_RENDER_TARGET;
    rtd.CPUAccessFlags = 0;
    rtd.MiscFlags = 0;
    ID3D11Texture2D* texture;
    device->CreateTexture2D(&rtd, NULL, &texture);
    D3D11_RENDER_TARGET_VIEW_DESC rtvd = {};
    rtvd.Format = rtd.Format;
    rtvd.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    rtvd.Texture2D.MipSlice = 0;

    RenderTargetResource rts = {};
    rts.texture = texture;
    device->CreateRenderTargetView(texture, &rtvd, &rts.view);
    RenderResource r = {};
    r.type = RenderResourceType::RenderTarget;
    r.render_target = rts;
    resources[handle] = r;

    RenderTarget rt = {};
    rt.render_resource = {handle};
    rt.pixel_format = pf;
    rt.width = rtd.Width;
    rt.height = rtd.Height;
    rt.clear = true;
    rt.clear_depth_stencil = true;
    rt.clear_color = {0, 0, 0, 1};
    return rt;
}

void Renderer::set_constant_buffers(const ConstantBuffer& data)
{
    D3D11_MAPPED_SUBRESOURCE ms_constant_buffer;
    device_context->Map(constant_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms_constant_buffer);
    memcpy(ms_constant_buffer.pData, &data, sizeof(ConstantBuffer));
    device_context->Unmap(constant_buffer, 0);
}

unsigned Renderer::find_free_resource_handle() const
{
    for (unsigned i = 1; i < num_resources; ++i)
    {
        if (resources[i].type == RenderResourceType::Unused)
        {
            return i;
        }
    }

    return InvalidHandle;
}

RRHandle Renderer::load_geometry(Vertex* vertices, unsigned num_vertices, unsigned* indices, unsigned num_indices)
{
    unsigned handle = find_free_resource_handle();

    if (handle == InvalidHandle)
        return {InvalidHandle};

    ID3D11Buffer* vertex_buffer;
    {
        D3D11_BUFFER_DESC bd = {0};
        bd.Usage = D3D11_USAGE_DYNAMIC;
        bd.ByteWidth = sizeof(Vertex) * num_vertices;
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        D3D11_SUBRESOURCE_DATA srd = {0};
        srd.pSysMem = vertices;
        device->CreateBuffer(&bd, &srd, &vertex_buffer);
    }

    ID3D11Buffer* index_buffer;
    {
        D3D11_BUFFER_DESC bd = {0};
        bd.Usage = D3D11_USAGE_DYNAMIC;
        bd.ByteWidth = sizeof(unsigned) * num_indices;
        bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        D3D11_SUBRESOURCE_DATA srd = {0};
        srd.pSysMem = indices;
        device->CreateBuffer(&bd, &srd, &index_buffer);
    }

    Geometry g = {0};
    g.vertices = vertex_buffer;
    g.indices = index_buffer;
    g.num_indices = num_indices;

    RenderResource r;
    r.type = RenderResourceType::Geometry;
    r.geometry = g;
    resources[handle] = r;
    return {handle};
}

void Renderer::unload_resource(RRHandle handle)
{
    RenderResource& res = get_resource(handle);

    switch (res.type)
    {
        case RenderResourceType::Geometry:
            res.geometry.vertices->Release();
            res.geometry.indices->Release();
            break;

        case RenderResourceType::Texture:
            res.texture.view->Release();
            res.texture.resource->Release();
            break;

        case RenderResourceType::Shader:
            res.shader.input_layout->Release();
            res.shader.vertex_shader->Release();
            res.shader.pixel_shader->Release();
            break;

        case RenderResourceType::RenderTarget:
            res.render_target.view->Release();

            if (res.render_target.texture != nullptr)
                res.render_target.texture->Release();
            
            break;

        default:
            if (res.type != RenderResourceType::Unused)
            {
                Error("Missing resource unloader.");
            }
            break;
    }

    memset(&res, 0, sizeof(RenderResource));
}

void Renderer::set_render_target(RenderTarget* rt)
{
    set_render_targets(&rt, 1);
}

void Renderer::set_render_targets(RenderTarget** rts, unsigned num)
{
    ID3D11RenderTargetView* targets[4];
    for (unsigned i = 0; i < num; ++i)
    {
        targets[i] = get_resource(rts[i]->render_resource).render_target.view;
    }
    device_context->OMSetRenderTargets(num, targets, depth_stencil_view);
    memset(render_targets, 0, sizeof(render_targets));
    memcpy(render_targets, rts, sizeof(RenderTarget**) * num);
}

void Renderer::draw(const Object& object, const Matrix4x4& view_matrix, const Matrix4x4& projection_matrix)
{
    auto geometry = get_resource(object.geometry_handle).geometry;
    ConstantBuffer constant_buffer_data = {};
    constant_buffer_data.model_view_projection = object.world_transform * view_matrix * projection_matrix;
    constant_buffer_data.model = object.world_transform;
    constant_buffer_data.projection = projection_matrix;
    set_constant_buffers(constant_buffer_data);
    device_context->VSSetConstantBuffers(0, 1, &constant_buffer);
    device_context->PSSetConstantBuffers(0, 1, &constant_buffer);

    if (IsValidRRHandle(object.lightmap_handle))
    {
        device_context->PSSetShaderResources(0, 1, &get_resource(object.lightmap_handle).texture.view);
    }

    unsigned stride = sizeof(Vertex);
    unsigned offset = 0;
    device_context->IASetVertexBuffers(0, 1, &geometry.vertices, &stride, &offset);
    device_context->IASetIndexBuffer(geometry.indices, DXGI_FORMAT_R32_UINT, 0);
    device_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    device_context->DrawIndexed(geometry.num_indices, 0, 0);
}

void Renderer::clear_depth_stencil()
{
    device_context->ClearDepthStencilView(depth_stencil_view, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
}

void Renderer::clear_render_target(RenderTarget* sc, const Color& color)
{
    device_context->ClearRenderTargetView(get_resource(sc->render_resource).render_target.view, &color.r);
}

void Renderer::present()
{
    swap_chain->Present(0, 0);
}

MappedTexture Renderer::map_texture(const RenderTarget& rt)
{
    D3D11_TEXTURE2D_DESC rtd = {};
    ID3D11Texture2D* texture = get_resource(rt.render_resource).render_target.texture;
    texture->GetDesc(&rtd);
    rtd.Usage = D3D11_USAGE_STAGING;
    rtd.BindFlags = 0;
    rtd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ID3D11Texture2D* staging_texture;
    device->CreateTexture2D(&rtd, NULL, &staging_texture);
    device_context->CopyResource(staging_texture, texture);
    D3D11_MAPPED_SUBRESOURCE mapped_resource;
    device_context->Map(staging_texture, 0, D3D11_MAP_READ, 0, &mapped_resource);
    MappedTexture m = {};
    m.data = mapped_resource.pData;
    m.texture = staging_texture;
    return m;
}

void Renderer::unmap_texture(const MappedTexture& m)
{
    device_context->Unmap(m.texture, 0);
    m.texture->Release();
}

void Renderer::pre_draw_frame()
{
    bool clear_depth = false;
    for (unsigned i = 0; i < max_render_targets; ++i)
    {
        RenderTarget* r = render_targets[i];

        if (r == nullptr)
        {
            continue;
        }

        if (r->clear)
        {
            clear_render_target(r, r->clear_color);
        }

        if (r->clear_depth_stencil)
        {
            clear_depth = true;
        }
    }

    if (clear_depth)
    {
        clear_depth_stencil();
    }
}

void Renderer::draw_frame(const World& world, const Camera& camera, DrawLights draw_lights)
{
    pre_draw_frame();
    Matrix4x4 view_matrix = camera::calc_view_matrix(camera);

    for (unsigned i = 0; i < world.num_objects; ++i)
    {
        if (world.objects[i].valid)
            draw(world.objects[i], view_matrix, camera.projection_matrix);
    }

    if (draw_lights == DrawLights::DrawLights)
    {
        for (unsigned i = 0; i < world.num_lights; ++i)
        {
            if (world.lights[i].valid)
                draw(world.lights[i], view_matrix, camera.projection_matrix);
        }
    }

    present();
}

void Renderer::set_scissor_rect(const Rect& r)
{
    static D3D11_RECT rect = {};
    rect.top = r.top;
    rect.bottom = r.bottom;
    rect.left = r.left;
    rect.right = r.right;
    device_context->RSSetScissorRects(1, &rect);
}

void Renderer::disable_scissor()
{
    D3D11_RECT rect = {};
    rect.top = 0;
    rect.bottom = WindowHeight;
    rect.left = 0;
    rect.right = WindowWidth;
    device_context->RSSetScissorRects(1, &rect);
}

RRHandle Renderer::load_texture(Allocator* allocator, wchar* filename)
{
    unsigned handle = find_free_resource_handle();

    if (handle == InvalidHandle)
        return {InvalidHandle};

    LoadedFile loaded_texture = file::load(allocator, filename);

    if (!loaded_texture.valid)
        return {InvalidHandle};

    File texture_file = loaded_texture.file;

    D3D11_TEXTURE2D_DESC desc;
    desc.Width = 200;
    desc.Height = 200;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;

    D3D11_SUBRESOURCE_DATA init_data;
    init_data.pSysMem = texture_file.data;
    init_data.SysMemPitch = 200 * image::pixel_size(PixelFormat::R8G8B8A8_UINT_NORM);
    init_data.SysMemSlicePitch = texture_file.size;

    ID3D11Texture2D* tex;
    if (device->CreateTexture2D(&desc, &init_data, &tex) != S_OK)
        return {InvalidHandle};

    ID3D11ShaderResourceView* resource_view;

    if (device->CreateShaderResourceView(tex, nullptr, &resource_view) != S_OK)
        return {InvalidHandle};

    RenderResource r;
    r.type = RenderResourceType::Texture;
    r.texture.resource = tex;
    r.texture.view = resource_view;
    resources[handle] = r;
    return {handle};
}

RenderResource& Renderer::get_resource(RRHandle r)
{
    Assert(r.h > 0 && r.h < num_resources, "Resource handle out of bounds.");
    return resources[r.h];
}
