#include <cstring>

struct __attribute__((packed)) v3f {
    float x, y, z;
};

class Box {
public:
    Mesh mesh;
    BoundingBox modelBox;
    explicit Box(Mesh mesh) : mesh(mesh), modelBox(GetMeshBoundingBox(mesh))
    {
    }

    BoundingBox boundingBox(Vector3 position, Vector3 rotationAxis, float rotationAngle, Vector3 scale) const
    {
        Matrix matScale = MatrixScale(scale.x, scale.y, scale.z);
        Matrix matRotation = MatrixRotate(rotationAxis, rotationAngle*DEG2RAD);
        Matrix matTranslation = MatrixTranslate(position.x, position.y, position.z);

        Matrix matTransform = MatrixMultiply(MatrixMultiply(matScale, matRotation), matTranslation);
        return {
            .min = Vector3Transform(modelBox.min, matTransform),
            .max = Vector3Transform(modelBox.max, matTransform),
        };
    }
};

class Cloth {
    float *ori = nullptr;
    float *mori = nullptr;
    bool wasColliding = false;
public:
    Mesh mesh;
    BoundingBox modelBox;

    explicit Cloth(Mesh mesh) : mesh(mesh), modelBox(GetMeshBoundingBox(mesh))
    {
        const auto l = mesh.vertexCount * 3 * sizeof(float);
        ori = (float *) malloc(l);
        mori = (float *) malloc(l);
        memcpy(ori, mesh.vertices, l);
    }

    void to(Matrix transform)
    {
        memcpy(mesh.vertices, ori, mesh.vertexCount * 3 * sizeof(float));
        for (int i = 0; i < mesh.vertexCount; i++) {
            Vector3 v = Vector3Transform({mesh.vertices[3*i], mesh.vertices[3*i + 1], mesh.vertices[3*i + 2]}, transform);
            mesh.vertices[3*i] = v.x;
            mesh.vertices[3*i + 1] = v.y;
            mesh.vertices[3*i + 2] = v.z;
        }
        glBindBuffer(GL_ARRAY_BUFFER, mesh.vboId[0]);
        glBufferData(GL_ARRAY_BUFFER, mesh.vertexCount * 3 * sizeof(float), mesh.vertices, GL_DYNAMIC_DRAW);
        memcpy(mori, mesh.vertices, mesh.vertexCount * 3 * sizeof(float));
    }

    void collide(Matrix transform, BoundingBox box)
    {
        auto wBox = BoundingBox {
            .min = Vector3Transform(modelBox.min, transform),
            .max = Vector3Transform(modelBox.max, transform),
        };
        if (!CheckCollisionBoxes(box, wBox)) {
            if (wasColliding) {
                memcpy(mesh.vertices, mori, mesh.vertexCount * 3 * sizeof(float));
                wasColliding = false;
                glBindBuffer(GL_ARRAY_BUFFER, mesh.vboId[0]);
                glBufferData(GL_ARRAY_BUFFER, mesh.vertexCount * 3 * sizeof(float), mesh.vertices, GL_DYNAMIC_DRAW);
            }
            return;
        }

        memcpy(mesh.vertices, mori, mesh.vertexCount * 3 * sizeof(float));

        // this algo sort of restricted the collision on a single plane, bad!
        auto *vertices = (v3f *) mesh.vertices;
        for (int i = 0; i < mesh.vertexCount; i++) {
            auto& v = vertices[i];
            if (v.y >= box.min.y && v.y <= box.max.y &&
                v.z <= box.min.z && v.z >= box.max.z)
                v.x = box.min.x - 0.1f;

            // heuristically apply dist minimizer on the same y column below the area of collision
            else if (v.y < box.max.y) {
                if (v.z <= box.min.z && v.z >= box.max.z)
                    v.x = box.min.x - 0.1f;
                else if (v.z > box.min.z)
                    v.x = (box.min.x - 0.1f) * ((wBox.max.z - v.z) / (wBox.max.z - box.min.z));
                else if (v.z < box.max.z)
                    v.x = (box.min.x - 0.1f) * ((v.z - wBox.min.z) / (box.max.z - wBox.min.z));
            }
        }
        wasColliding = true;
        glBindBuffer(GL_ARRAY_BUFFER, mesh.vboId[0]);
        glBufferData(GL_ARRAY_BUFFER, mesh.vertexCount * 3 * sizeof(float), mesh.vertices, GL_DYNAMIC_DRAW);
    }

    ~Cloth() {
        if (ori)
            free(ori);
        if (mori)
            free(mori);
    }
};