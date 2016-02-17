#include "renderer.h"
#include "dynamic_array.h"

struct Mesh
{
    DynamicArray<Vertex> vertices;
    DynamicArray<uint32> indices;
};

struct LoadedMesh
{
    bool valid;
    Mesh mesh;
};

struct ParserState
{
    uint8* data;
    uint8* head;
    uint8* end;
};

namespace obj
{

namespace internal
{

struct ParsedFace
{
    uint32 v1;
    uint32 v2;
    uint32 v3;
    uint32 n1;
    uint32 n2;
    uint32 n3;
    uint32 u1;
    uint32 u2;
    uint32 u3;
};

struct ParsedData
{
    DynamicArray<Vector3> vertices;
    DynamicArray<Vector3> normals;
    DynamicArray<Vector2> uvs;
    DynamicArray<ParsedFace> faces;
};

void skip_to_numeric(ParserState* ps)
{
    while (ps->head < ps->end && (*ps->head < '0' || *ps->head > '9') && *ps->head != '-')
    {
        ++ps->head;
    }
}

void parse_uv(ParserState* ps, ParsedData* pd)
{
    Vector2* uv = pd->uvs.push();
    skip_to_numeric(ps);
    uv->x = strtof((const char*)ps->head, (char**)&ps->head);
    skip_to_numeric(ps);
    uv->y = strtof((const char*)ps->head, (char**)&ps->head);
}

void parse_normal(ParserState* ps, ParsedData* pd)
{
    Vector3* normal = pd->normals.push();
    skip_to_numeric(ps);
    normal->x = strtof((const char*)ps->head, (char**)&ps->head);
    skip_to_numeric(ps);
    normal->y = strtof((const char*)ps->head, (char**)&ps->head);
    skip_to_numeric(ps);
    normal->z = strtof((const char*)ps->head, (char**)&ps->head);
}

void parse_vertex(ParserState* ps, ParsedData* pd)
{
    Vector3* vertex = pd->vertices.push();
    skip_to_numeric(ps);
    vertex->x = strtof((const char*)ps->head, (char**)&ps->head);
    skip_to_numeric(ps);
    vertex->y = strtof((const char*)ps->head, (char**)&ps->head);
    skip_to_numeric(ps);
    vertex->z = strtof((const char*)ps->head, (char**)&ps->head);
}

void parse_face(ParserState* ps, ParsedData* pd)
{
    ParsedFace* face = pd->faces.push();
    skip_to_numeric(ps);
    face->v1 = strtol((const char*)ps->head, (char**)&ps->head, 10) - 1;
    skip_to_numeric(ps);
    face->u1 = strtol((const char*)ps->head, (char**)&ps->head, 10) - 1;
    skip_to_numeric(ps);
    face->n1 = strtol((const char*)ps->head, (char**)&ps->head, 10) - 1;
    skip_to_numeric(ps);
    face->v2 = strtol((const char*)ps->head, (char**)&ps->head, 10) - 1;
    skip_to_numeric(ps);
    face->u2 = strtol((const char*)ps->head, (char**)&ps->head, 10) - 1;
    skip_to_numeric(ps);
    face->n2 = strtol((const char*)ps->head, (char**)&ps->head, 10) - 1;
    skip_to_numeric(ps);
    face->v3 = strtol((const char*)ps->head, (char**)&ps->head, 10) - 1;
    skip_to_numeric(ps);
    face->u3 = strtol((const char*)ps->head, (char**)&ps->head, 10) - 1;
    skip_to_numeric(ps);
    face->n3 = strtol((const char*)ps->head, (char**)&ps->head, 10) - 1;
}

void skip_line(ParserState* ps)
{
    while (ps->head < ps->end && *ps->head != '\n')
    {
        ++ps->head;
    }

    ++ps->head;
}

ParsedData parse(Allocator* alloc, uint8* data, uint32 data_size)
{
    ParserState ps = {};
    ps.data = data;
    ps.head = data;
    ps.end = (uint8*)memory::ptr_add(data, data_size);
    ParsedData pd = {};
    pd.vertices = {alloc};
    pd.normals = {alloc};
    pd.uvs = {alloc};
    pd.faces = {alloc};
    ps.head = ps.data;

    while (ps.head < ps.end)
    {
        uint8 c = *ps.head;
        bool first_on_line = ps.head == ps.data || (ps.head > ps.data && (*(ps.head - 1)) == '\n');

        if (!first_on_line)
            skip_line(&ps);
        else if (c == 'v' && ps.head + 1 < ps.end && (*(ps.head+1)) == 't')
            parse_uv(&ps, &pd);
        else if (c == 'v' && ps.head + 1 < ps.end && (*(ps.head+1)) == 'n')
            parse_normal(&ps, &pd);
        else if (c == 'v')
            parse_vertex(&ps, &pd);
        else if (c == 'f')
            parse_face(&ps, &pd);
        else
            skip_line(&ps);
    }

    return pd;
}

int32 get_existing_vertex(const DynamicArray<Vertex>& vertices, const Vertex& v1)
{
    for (uint32 i = 0; i < vertices.num; ++i)
    {
        const Vertex& v2 = vertices[i];

        if (vector3::almost_equal(v1.position, v2.position)
            && vector3::almost_equal(v1.normal, v2.normal)
            && vector2::almost_equal(v1.uv, v2.uv)
            && color::almost_equal(v1.color, v2.color))
        {
            return i;
        }
    }

    return -1;
}

void add_vertex_to_mesh(Mesh* m, const Vertex& v)
{
    int32 i = get_existing_vertex(m->vertices, v);

    if (i != -1)
    {
        m->indices.add(i);
        return;
    }

    m->indices.add(m->vertices.num);
    m->vertices.add(v);
}

}

LoadedMesh load(Allocator* alloc, const char* filename)
{
    LoadedFile lf = file::load(alloc, filename);

    if (!lf.valid)
        return {false};

    internal::ParsedData pd = internal::parse(alloc, lf.file.data, lf.file.size);
    Mesh m = {};
    m.vertices = {alloc};
    m.indices = {alloc};

    for (uint32 i = 0; i < pd.faces.num; ++i)
    {
        const internal::ParsedFace& f = pd.faces[i];
        internal::add_vertex_to_mesh(&m, {pd.vertices[f.v1], pd.normals[f.n1], pd.uvs[f.u1], {1.0f, 1.0f, 1.0f, 1.0f}});
        internal::add_vertex_to_mesh(&m, {pd.vertices[f.v2], pd.normals[f.n2], pd.uvs[f.u2], {1.0f, 1.0f, 1.0f, 1.0f}});
        internal::add_vertex_to_mesh(&m, {pd.vertices[f.v3], pd.normals[f.n3], pd.uvs[f.u3], {1.0f, 1.0f, 1.0f, 1.0f}});
    }

    return {true, m};
}

}