#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#define GLM_FORCE_SWIZZLE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <fstream>
#include <iostream>
#include <vector>
#include <random>
#include <set>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

using namespace glm;

GLFWwindow *window = nullptr;

int dw, dh;

void debugMessage(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length,
                  const GLchar *message, const void *userParam)
{
    std::cerr << message << std::endl;
}

bool initGLandIMGUI()
{
    glfwSetErrorCallback([](int error, const char *description)
                         {
                             std::cerr << "GLFW ERROR " << error << ": " << description << std::endl;
                         });
    if (!glfwInit())
        return false;

    const char *glsl_version = "#version 430 core";
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    window = glfwCreateWindow(1280, 720, "ROBOT", nullptr, nullptr);
    if (!window)
        return false;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    auto &io = ImGui::GetIO();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    glewInit();

    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDebugMessageCallback(debugMessage, NULL);
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);

    return true;
}

// print the compile log of an OpenGL shader object, if GL_COMPILE_STATUS is GL_FALSE
void printGLShaderLog(const GLuint shader)
{
    GLint isCompiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);
    if (isCompiled == GL_FALSE) {
        GLint maxLength = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);

        // maxLength already includes the NULL terminator. no need to +1
        auto *errorLog = new GLchar[maxLength];
        glGetShaderInfoLog(shader, maxLength, &maxLength, &errorLog[0]);

        printf("%s\n", errorLog);
        delete[] errorLog;
    }
}

std::string readFile(const std::string &fn)
{
    std::ifstream f(fn);
    if (!f)
        throw std::runtime_error("Can't load file " + fn);
    std::stringstream s;
    s << f.rdbuf();
    f.close();
    return s.str();
}

struct Mesh
{
    GLuint vao = 0;
    int vtx_count = 0;
    int idx_count = 0;

    Mesh(aiMesh *m)
    {
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);

        GLuint bufs[4];
        glGenBuffers(4, bufs);

        vtx_count = m->mNumVertices;

        glBindBuffer(GL_ARRAY_BUFFER, bufs[0]);
        glBufferData(GL_ARRAY_BUFFER, vtx_count * sizeof(float) * 3, m->mVertices, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
        glEnableVertexAttribArray(0);

        glBindBuffer(GL_ARRAY_BUFFER, bufs[1]);
        glBufferData(GL_ARRAY_BUFFER, vtx_count * sizeof(float) * 3, m->mTextureCoords[0], GL_STATIC_DRAW);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);
        glEnableVertexAttribArray(1);

        glBindBuffer(GL_ARRAY_BUFFER, bufs[2]);
        glBufferData(GL_ARRAY_BUFFER, vtx_count * sizeof(float) * 3, m->mNormals, GL_STATIC_DRAW);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, 0);
        glEnableVertexAttribArray(2);

        std::vector<unsigned int> indices;
        for (unsigned int i = 0; i < m->mNumFaces; i++) {
            auto f = m->mFaces[i];
            for (unsigned j = 0; j < f.mNumIndices; j++)
                indices.push_back(f.mIndices[j]);
        }
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bufs[3]);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
        idx_count = indices.size();
    }
};

struct Node
{
    aiNode *n;
    aiMatrix4x4 t;

    Node(aiNode *n, aiMatrix4x4 t) : n(n), t(t) {}
};

struct Texture
{
    int w = 0, h = 0;
    GLuint m_texture = 0;
    vec4 color = vec4(0.0f);

    Texture() = default;

    explicit Texture(const char *path)
    {
        stbi_set_flip_vertically_on_load(false);
        int comp;
        auto *raw = stbi_load(path, &w, &h, &comp, 4);
        if (!raw)
            throw std::runtime_error("Can't load texture " + std::string(path));
        auto size = w * h * 4;
        std::vector<uint8_t> data;
        data.reserve(size);
        for (size_t i = 0; i < size; i++)
            data.push_back(raw[i]);
        stbi_image_free(raw);

        glGenTextures(1, &m_texture);
        glBindTexture(GL_TEXTURE_2D, m_texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
};

class Program
{
public:
    GLuint program = glCreateProgram();
    std::string name;

    Program(const char *vtx, const char *frg)
    {
        auto fsrc = readFile(frg);
        const char *frag_shader[] = {fsrc.data()};
        auto fs = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fs, 1, frag_shader, nullptr);
        glCompileShader(fs);

        auto vsrc = readFile(vtx);
        const char *vert_shader[] = {vsrc.data()};
        auto vs = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vs, 1, vert_shader, nullptr);
        glCompileShader(vs);

        glAttachShader(program, vs);
        printGLShaderLog(vs);
        glAttachShader(program, fs);
        printGLShaderLog(fs);
        glLinkProgram(program);
    }
};


double t = 0;
class App
{
    Program original = Program("vertex.glsl", "fragment.glsl");
    GLint m, vp, ov;
    const aiScene *s = nullptr;
    std::unordered_map<int, Mesh> meshes;
    std::unordered_map<int, Texture> textures;
    std::vector<Node> nodes;
    Assimp::Importer importer;

    // temp
    std::set<int> mat_idx;

    vec3 cpos = vec3(0, 0.5, 0);
    vec3 direction;
    double yaw = 5.66;
    double pitch = 0;

    const float factor = 0.2f;

    GLuint oprogram = original.program;

public:
    App()
    {
        vp = glGetUniformLocation(oprogram, "vp");
        m = glGetUniformLocation(oprogram, "m");
        ov = glGetUniformLocation(oprogram, "ov_color");

        s = importer.ReadFile("assets/Grey_White_Room.obj", aiProcess_Triangulate);
        if (!s || s->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !s->mRootNode)
            throw std::runtime_error("ASSIMP LFAILED");

        importNode(s->mRootNode, aiMatrix4x4());

        for (auto midx: mat_idx) {
            const auto mat = s->mMaterials[midx];
            aiString str;
            if (mat->GetTextureCount(aiTextureType_DIFFUSE) == 0) {
                std::cout << midx << "is 0 !\n";
            }

            if (mat->GetTexture(aiTextureType_DIFFUSE, 0, &str) == aiReturn_SUCCESS) {
                auto path = "assets/" + std::string(str.C_Str());
                textures.emplace(midx, Texture(path.c_str()));
            }
            else {
                aiString str2;
                mat->Get(AI_MATKEY_NAME, str2);
                std::cout << str2.C_Str() << std::endl;

                float f[3] = {0};
                aiGetMaterialFloatArray(mat, AI_MATKEY_COLOR_DIFFUSE, f, NULL);
                Texture t;
                t.color = vec4(f[0], f[1], f[2], 1.0f);
                textures.emplace(midx, t);
            }
        }

        direction.x = cos(yaw) * cos(pitch);
        direction.y = sin(pitch);
        direction.z = sin(yaw) * cos(pitch);
        direction = normalize(direction);
    }

    void importNode(aiNode *node, aiMatrix4x4 t)
    {
        auto cur_t = t * node->mTransformation;
        for (unsigned int i = 0; i < node->mNumMeshes; i++) {
            auto idx = node->mMeshes[i];
            meshes.emplace(idx, Mesh(s->mMeshes[idx]));
            mat_idx.insert(s->mMeshes[idx]->mMaterialIndex);
        }
        nodes.emplace_back(node, cur_t);

        for (unsigned int i = 0; i < node->mNumChildren; i++)
            importNode(node->mChildren[i], cur_t);
    }

    void frame(double dt)
    {
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            auto dlt = ImGui::GetMouseDragDelta();
            yaw += dlt.x / 500.0f;
            pitch -= dlt.y / 500.0f;
            direction.x = cos(yaw) * cos(pitch);
            direction.y = sin(pitch);
            direction.z = sin(yaw) * cos(pitch);
            direction = normalize(direction);
        }
        if (ImGui::IsKeyReleased(ImGuiKey_W))
            cpos += factor * direction;
        if (ImGui::IsKeyReleased(ImGuiKey_S))
            cpos -= factor * direction;
        if (ImGui::IsKeyReleased(ImGuiKey_A))
            cpos -= normalize(cross(direction, vec3(0, 1, 0))) * factor;
        if (ImGui::IsKeyReleased(ImGuiKey_D))
            cpos += normalize(cross(direction, vec3(0, 1, 0))) * factor;
        if (ImGui::IsKeyReleased(ImGuiKey_Q))
            cpos.y += factor;
        if (ImGui::IsKeyReleased(ImGuiKey_Z))
            cpos.y -= factor;

        mat4 p = perspective(radians(45.0f), (float) dw / (float) dh, 0.1f, 1000.0f);
        mat4 v = lookAt(cpos, cpos + direction, vec3(0, 1, 0));

        glUseProgram(oprogram);
        glUniformMatrix4fv(vp, 1, GL_FALSE, value_ptr(p * v));

        glViewport(0, 0, dw, dh);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        for (auto &node: nodes) {
            for (unsigned int i = 0; i < node.n->mNumMeshes; i++) {
                glUniformMatrix4fv(m, 1, GL_FALSE, (GLfloat *) &node.t);
                auto midx = s->mMeshes[node.n->mMeshes[i]]->mMaterialIndex;
                const auto &texture = textures.at(s->mMeshes[node.n->mMeshes[i]]->mMaterialIndex);
                const auto &mesh = meshes.at(node.n->mMeshes[i]);
                glUniform4fv(ov, 1, value_ptr(texture.color));
                glBindTexture(GL_TEXTURE_2D, texture.m_texture);
                glBindVertexArray(mesh.vao);
                glDrawElements(GL_TRIANGLES, mesh.idx_count, GL_UNSIGNED_INT, nullptr);
            }
        }
    }
};

int main()
{
    if (!initGLandIMGUI())
        return 1;
    glfwGetFramebufferSize(window, &dw, &dh);
    glfwSetCursorPos(window, dw / 2, dh / 2);
    std::cerr << dw << ' ' << dh << std::endl;

    App app;

    t = glfwGetTime();
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (ImGui::IsKeyReleased(ImGuiKey_Escape))
            glfwSetWindowShouldClose(window, true);

        glfwGetFramebufferSize(window, &dw, &dh);

        auto dt = glfwGetTime() - t;
        t += dt;
        app.frame(dt);

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    return 0;
}
