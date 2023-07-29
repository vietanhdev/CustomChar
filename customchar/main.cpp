#include <stdio.h>
#include <iostream>
#include <mutex>
#include <thread>

#include <semaphore>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#if defined(__APPLE__)
#define GL_SILENCE_DEPRECATION  // Silence deprecation warnings on macos
#endif
#include <GLFW/glfw3.h>  // Will drag system OpenGL headers

#include "customchar/character/character.h"
#include "customchar/common/common.h"
#include "customchar/common/helpers.h"
#include "customchar/session/chat_history.h"
#include "customchar/session/chat_message.h"
#include "customchar/vision/video_capture.h"

#include "imgui_internal.h"
#include "imspinner/imspinner.h"

using namespace CC;
using namespace CC::character;

vision::VideoCapture video_capture;

// [Win32] Our example includes a copy of glfw3.lib pre-compiled with VS2010 to
// maximize ease of testing and compatibility with old VS compilers. To link
// with VS2010-era libraries, VS2015+ requires linking with
// legacy_stdio_definitions.lib, which we do using this pragma. Your own project
// should not be affected, as you are likely to link with a newer binary of GLFW
// that is adequate for your version of Visual Studio.
#if defined(_MSC_VER) && (_MSC_VER >= 1900) && \
    !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")
#endif

// Global variables
// synchronize
// everytime user sends message, IMGUI sets global variable to message
// signal client server ... lock/unlock mutex
constexpr int TEXT_MESSAGE_SIZE = 1024 * 8;
constexpr int INIT_WINDOW_WIDTH = 600;
constexpr int INIT_WINDOW_HEIGHT = 600;

static void GLFWErrorCallback(int error, const char* description) {
  fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

std::mutex mtx;
bool OnNewMessage(const std::string& text, const std::string& sender,
                  std::shared_ptr<session::ChatHistory> history) {
  mtx.lock();
  history->AddMessage(text, sender);
  mtx.unlock();
  return true;
}

/**
 * @brief Main ImGUI loop
 */
void runImgui(std::shared_ptr<session::ChatHistory> history) {
  // Setup window
  glfwSetErrorCallback(GLFWErrorCallback);
  if (!glfwInit()) return;

// Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
  // GL ES 2.0 + GLSL 100
  const char* glsl_version = "#version 100";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
  glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(__APPLE__)
  // GL 3.2 + GLSL 150
  const char* glsl_version = "#version 150";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
#else
  // GL 3.0 + GLSL 130
  const char* glsl_version = "#version 130";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
  // glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+
  // only glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // 3.0+ only
#endif

  // Create window with graphics context
  GLFWwindow* window = glfwCreateWindow(INIT_WINDOW_WIDTH, INIT_WINDOW_HEIGHT,
                                        "CustomChar", NULL, NULL);
  if (window == NULL) return;
  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);  // Enable vsync

  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  (void)io;
  // io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable
  // Keyboard Controls io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad; //
  // Enable Gamepad Controls

  // Setup Dear ImGui style
  // ImGui::StyleColorsDark();
  ImGui::StyleColorsDark();

  // Setup Platform/Renderer backends
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init(glsl_version);

  // Load Font
  ImFont* font =
      io.Fonts->AddFontFromFileTTF("fonts/BaiJamjuree-Medium.ttf", 18.0f, NULL,
                                   io.Fonts->GetGlyphRangesJapanese());
  IM_ASSERT(font != NULL);

  // Our state
  ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
  bool justSent = true;

  // Initial text
  char text[TEXT_MESSAGE_SIZE] = "";

  GLuint texture;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

  // Main loop
  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();

    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    int TEXTBOX_HEIGHT = 40;

    // Make window take up full system window
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);

    // Create window
    ImGui::Begin("CustomChar", NULL,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);

    // Get window size
    ImVec2 windowSize = ImGui::GetWindowSize();

    // Resize image to fit window
    cv::Mat image = video_capture.GetFrame();
    cv::Mat resized_image;
    float ratio = (float)image.cols / (float)image.rows;
    int new_width = windowSize.x;
    int new_height = new_width / ratio;
    cv::resize(image, resized_image, cv::Size(new_width, new_height));
    cv::cvtColor(resized_image, resized_image, cv::COLOR_BGR2RGBA);

    // Display image
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, resized_image.cols,
                 resized_image.rows, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 resized_image.data);
    ImGui::Image(reinterpret_cast<void*>(static_cast<intptr_t>(texture)),
                 ImVec2(resized_image.cols, resized_image.rows), ImVec2(0, 0),
                 ImVec2(1, 1), ImColor(255, 255, 255, 255),
                 ImColor(255, 255, 255, 128));

    // Child window scrollable area
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_None;

    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.0f);
    ImGui::BeginChild(
        "ChildR",
        ImVec2(0, ImGui::GetContentRegionAvail().y - TEXTBOX_HEIGHT - 20), true,
        window_flags);

    // TODO: Format chat history
    ImGui::Dummy(ImVec2(0, ImGui::GetContentRegionAvail().y));

    for (session::ChatMessage message : history->GetCharHistory()) {
      ImGui::Spacing();
      // ImGui::TextWrapped("%s", message.GetTime().c_str());
      ImGui::TextWrapped("> %s: %s", message.GetSender().c_str(),
                         message.GetMessage().c_str());
    }
    if (history->HasNewMessage() || justSent) {
      ImGui::SetScrollHereY(1.0f);
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();

    // Text input area flags
    // Prevent user from inputting characters
    ImGuiInputTextFlags input_flags = ImGuiInputTextFlags_ReadOnly;

    // Refocus text area if text was just sent
    if (justSent) {
      ImGui::SetKeyboardFocusHere();
      justSent = false;
    }

    // Create a spinner and text input in the same line
    ImGui::PushItemWidth(32);
    static float velocity = 1.f;
    ImColor color(255, 255, 255);
    ImSpinner::SpinnerHerbertBalls3D("Spinner", 24, 3.0f, color,
                                     1.4f * velocity);
    ImGui::PopItemWidth();
    ImGui::SameLine();
    strcpy(text, "Say something...");
    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
    if (ImGui::InputText("##source", text, IM_ARRAYSIZE(text), input_flags)) {
      justSent = OnNewMessage(text, "User", history);
    };

    // Put the cursor of InputTextMultiline at the end of the text
    ImGui::SetKeyboardFocusHere();
    ImGui::End();

    // Rendering
    ImGui::Render();
    int display_w, display_h;
    glfwGetFramebufferSize(window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w,
                 clear_color.z * clear_color.w, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(window);
  }

  glDeleteTextures(1, &texture);
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  glfwDestroyWindow(window);
  glfwTerminate();
}

int main(int argc, char** argv) {
  // Parse command line arguments
  common::CCParams params;
  if (CCParamsParse(argc, argv, params) == false) {
    exit(1);
  }
  if (whisper_lang_id(params.language.c_str()) == -1) {
    fprintf(stderr, "error: unknown language '%s'\n", params.language.c_str());
    CCPrintUsage(argc, argv, params);
    exit(1);
  }

  // Initialize chat history
  std::shared_ptr<session::ChatHistory> history =
      std::make_shared<session::ChatHistory>();

  // Create character
  Character character(params);

  video_capture.Start();

  // Set message callbacks
  character.SetOnUserMessage(
      std::bind(OnNewMessage, std::placeholders::_1, "User", history));
  character.SetOnBotMessage(
      std::bind(OnNewMessage, std::placeholders::_1, "CustomChar", history));

  // Start character in a new thread
  std::thread character_thread(&Character::Run, &character);
  character_thread.detach();

  // Main GUI loop
  runImgui(history);

  return 0;
}
