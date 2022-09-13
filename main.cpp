#include <iostream>

#include "glad/gl.h"

#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"

#include "geo.h"

#define APPNAME     "VPILOT"
#define CODENAME    "BLOOMING V"

Mesh gen(float width, float length, int resX, int resZ)
{
    Mesh mesh = { 0 };

    resX++;
    resZ++;

    // Vertices definition
    int vertexCount = resX*resZ; // vertices get reused for the faces

    Vector3 *vertices = (Vector3 *)RL_MALLOC(vertexCount*sizeof(Vector3));
    for (int z = 0; z < resZ; z++)
    {
        // [-length/2, length/2]
        float zPos = ((float)z/(resZ - 1) - 0.5f)*length;
        for (int x = 0; x < resX; x++)
        {
            // [-width/2, width/2]
            float xPos = ((float)x/(resX - 1) - 0.5f)*width;
            vertices[x + z*resX] = (Vector3){ xPos, 0.0f, zPos };
        }
    }


    // Triangles definition (indices)
    int numFaces = (resX - 1)*(resZ - 1);
    int *triangles = (int *)RL_MALLOC(numFaces*6*sizeof(int));
    int t = 0;
    for (int face = 0; face < numFaces; face++)
    {
        // Retrieve lower left corner from face ind
        int i = face % (resX - 1) + (face/(resZ - 1)*resX);

        triangles[t++] = i + resX;
        triangles[t++] = i + 1;
        triangles[t++] = i;

        triangles[t++] = i + resX;
        triangles[t++] = i + resX + 1;
        triangles[t++] = i + 1;
    }

    mesh.vertexCount = vertexCount;
    mesh.triangleCount = numFaces*2;
    mesh.vertices = (float *)RL_MALLOC(mesh.vertexCount*3*sizeof(float));
    mesh.indices = (unsigned short *)RL_MALLOC(mesh.triangleCount*3*sizeof(unsigned short));

    // Mesh vertices position array
    for (int i = 0; i < mesh.vertexCount; i++)
    {
        mesh.vertices[3*i] = vertices[i].x;
        mesh.vertices[3*i + 1] = vertices[i].y;
        mesh.vertices[3*i + 2] = vertices[i].z;
    }

    // Mesh indices array initialization
    for (int i = 0; i < mesh.triangleCount*3; i++) mesh.indices[i] = triangles[i];

    RL_FREE(vertices);
    RL_FREE(triangles);
    return mesh;
}

void upload(Mesh *mesh) {
    const bool dynamic = true;
    mesh->vboId = (unsigned int *)RL_CALLOC(7, sizeof(unsigned int));
    mesh->vboId[0] = 0;     // Vertex buffer: positions
    mesh->vboId[1] = 0;     // Vertex buffer: texcoords
    mesh->vboId[2] = 0;     // Vertex buffer: normals
    mesh->vboId[3] = 0;     // Vertex buffer: colors
    mesh->vboId[4] = 0;     // Vertex buffer: tangents
    mesh->vboId[5] = 0;     // Vertex buffer: texcoords2
    mesh->vboId[6] = 0;     // Vertex buffer: indices

    mesh->vaoId = rlLoadVertexArray();
    rlEnableVertexArray(mesh->vaoId);

    // Enable vertex attributes: position (shader-location = 0)
    void *vertices = mesh->vertices;
    mesh->vboId[0] = rlLoadVertexBuffer(vertices, mesh->vertexCount*3*sizeof(float), dynamic);
    rlSetVertexAttribute(0, 3, RL_FLOAT, 0, 0, 0);
    rlEnableVertexAttribute(0);

    // Default vertex attribute: color
    // WARNING: Default value provided to shader if location available
    float value[4] = { 1.0f, 1.0f, 1.0f, 1.0f };    // WHITE
    rlSetVertexAttributeDefault(3, value, SHADER_ATTRIB_VEC4, 4);
    rlDisableVertexAttribute(3);

    mesh->vboId[6] = rlLoadVertexBufferElement(mesh->indices, mesh->triangleCount*3*sizeof(unsigned short), dynamic);

    rlDisableVertexArray();
}

void draw(const Material material, const Mesh mesh, const Matrix transform)
{
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); // EnableWireMode

        // Bind shader program
        rlEnableShader(material.shader.id);

        // Send required data to shader (matrices, values)
        //-----------------------------------------------------
        // Upload to shader material.colDiffuse
        float values[4] = {
                (float)material.maps[MATERIAL_MAP_DIFFUSE].color.r/255.0f,
                (float)material.maps[MATERIAL_MAP_DIFFUSE].color.g/255.0f,
                (float)material.maps[MATERIAL_MAP_DIFFUSE].color.b/255.0f,
                (float)material.maps[MATERIAL_MAP_DIFFUSE].color.a/255.0f
        };

        rlSetUniform(material.shader.locs[SHADER_LOC_COLOR_DIFFUSE], values, SHADER_UNIFORM_VEC4, 1);

        // Get a copy of current matrices to work with,
        // just in case stereo render is required and we need to modify them
        // NOTE: At this point the modelview matrix just contains the view matrix (camera)
        // That's because BeginMode3D() sets it and there is no model-drawing function
        // that modifies it, all use rlPushMatrix() and rlPopMatrix()
        Matrix matModel = MatrixIdentity();
        Matrix matView = rlGetMatrixModelview();
        Matrix matModelView = MatrixIdentity();
        Matrix matProjection = rlGetMatrixProjection();

        // Upload view and projection matrices (if locations available)
        if (material.shader.locs[SHADER_LOC_MATRIX_VIEW] != -1) rlSetUniformMatrix(material.shader.locs[SHADER_LOC_MATRIX_VIEW], matView);
        if (material.shader.locs[SHADER_LOC_MATRIX_PROJECTION] != -1) rlSetUniformMatrix(material.shader.locs[SHADER_LOC_MATRIX_PROJECTION], matProjection);

        // Model transformation matrix is send to shader uniform location: SHADER_LOC_MATRIX_MODEL
        if (material.shader.locs[SHADER_LOC_MATRIX_MODEL] != -1) rlSetUniformMatrix(material.shader.locs[SHADER_LOC_MATRIX_MODEL], transform);

        // Accumulate several model transformations:
        //    transform: model transformation provided (includes DrawModel() params combined with model.transform)
        //    rlGetMatrixTransform(): rlgl internal transform matrix due to push/pop matrix stack
        matModel = MatrixMultiply(transform, rlGetMatrixTransform());

        // Get model-view matrix
        matModelView = MatrixMultiply(matModel, matView);

        // Bind active texture maps (if available)
        const int i = 0;
        rlActiveTextureSlot(i);
        rlEnableTexture(material.maps[i].texture.id);
        rlSetUniform(material.shader.locs[SHADER_LOC_MAP_DIFFUSE + i], &i, SHADER_UNIFORM_INT, 1);

        rlEnableVertexArray(mesh.vaoId);

        // Calculate model-view-projection matrix (MVP)
        Matrix matModelViewProjection = MatrixMultiply(matModelView, matProjection);

        // Send combined model-view-projection matrix to shader
        rlSetUniformMatrix(material.shader.locs[SHADER_LOC_MATRIX_MVP], matModelViewProjection);

        glDrawElements(GL_TRIANGLES, mesh.triangleCount*3, GL_UNSIGNED_SHORT, 0);

        // Unbind all binded texture maps
        rlActiveTextureSlot(i);
        rlDisableTexture();

        // Disable all possible vertex array objects (or VBOs)
        rlDisableVertexArray();

        // Disable shader program
        rlDisableShader();

        // Restore rlgl internal modelview and projection matrices
        rlSetMatrixModelview(matView);
        rlSetMatrixProjection(matProjection);

        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); // DisableWireMode
}

int main()
{
    SetConfigFlags(FLAG_VSYNC_HINT);
    InitWindow(800, 600, APPNAME " - " CODENAME);

    Mesh mesh = gen(20.0f * 0.5f, 20.0f * 0.5f, 19, 19);
    upload(&mesh);

    Camera3D cam;
    cam.target = {1.5, -0.15, 0};
    cam.position = {-4.5, 0.1, 0};
    cam.up = {0, 1, 0};
    cam.fovy = 45;
    cam.projection = CAMERA_PERSPECTIVE;

    auto color = WHITE;
    auto tint = GREEN;
    auto colorTint = WHITE;
    colorTint.r = (unsigned char)((((float)color.r/255.0f)*((float)tint.r/255.0f))*255.0f);
    colorTint.g = (unsigned char)((((float)color.g/255.0f)*((float)tint.g/255.0f))*255.0f);
    colorTint.b = (unsigned char)((((float)color.b/255.0f)*((float)tint.b/255.0f))*255.0f);
    colorTint.a = (unsigned char)((((float)color.a/255.0f)*((float)tint.a/255.0f))*255.0f);

    auto material = LoadMaterialDefault();
    material.maps[MATERIAL_MAP_DIFFUSE].color = colorTint;

    Matrix scale = MatrixScale(0.25f, 0.25f, 0.25f);
    Matrix rot = MatrixRotate({0, 0, 1.0f}, 90.0f * DEG2RAD);
    Matrix trans = MatrixTranslate(0, 0, 0);
    Matrix transform = MatrixMultiply(MatrixMultiply(scale, rot), trans);

    Model rect = LoadModelFromMesh(GenMeshCube(5.0f * 0.5f, 5.0f * 0.5f, 10.0f));

    auto cloth = Cloth(mesh);
    auto box = Box(rect.meshes[0]);

    cloth.to(transform);

    SetCameraMode(cam, CAMERA_FREE);

    float p = 2.5f;

    while (!WindowShouldClose()) {
        UpdateCamera(&cam);

        if (IsKeyReleased(KEY_LEFT))
            p -= 0.1f;
        if (IsKeyReleased(KEY_RIGHT))
            p += 0.1f;

        cloth.collide(transform, box.boundingBox({p, 0, 0}, {0, 1, 0}, 90, {0.5f, 0.5f, 0.5f}));

        BeginDrawing();
        ClearBackground(RAYWHITE);
        BeginMode3D(cam);

        draw(material, cloth.mesh, MatrixIdentity());
        DrawModelEx(rect, {p, 0, 0}, {0, 1, 0}, 90, {0.5f, 0.5f, 0.5f}, LIGHTGRAY);

        DrawGrid(10, 1.0f);
        EndMode3D();

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
