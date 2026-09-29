#include "pch.h"
#include "../Console/ConsoleWindow.h"
#include "ImGui/imgui.h"
#include "Core/Channel/FEditorInfo.h"

#include <sstream>

// 로그 저장 시
using enum ELogLevel;
using enum ELogCategory;

namespace {
    const char* LogCategoryToString(ELogCategory Category) {
        switch (Category) {
            case ELogCategory::Core:
                return "[Core]";
            case ELogCategory::Render:
                return "[Render]";
            case ELogCategory::Physics:
                return "[Physics]";
            case ELogCategory::Etc:
                return "[Etc]";
            default:
                return "[Unknown]";
        }
    }

    void ExecuteCommand(FConsoleOutputHandle Handle, const char* Input, FStateChannel<FStatDisplayFlags>::FWriter Writer) {
        std::istringstream Stream{Input};

        FString Command{};
        //Stream >> Command;

        std::getline(Stream, Command);

        if (Command == "clear") {
            Console::Clear(Handle);
        } else if (Command == "help") {
            Console::AddLog(Handle, Log, Core, "Commands: clear, echo, error, stat fps, stat render, stat picking, stat memory, stat object system, stat none");
        } else if (Command == "error") {
            FString Text{};
            std::getline(Stream >> std::ws, Text);

            Console::AddLog(Handle, Error, Etc, "Error Test");
        } else if (Command == "echo") {
            FString Text{};
            std::getline(Stream >> std::ws, Text);

            Console::AddLog(Handle, Log, Core, "> %s", Text.c_str());
        } else if (Command == "stat fps") {
            Writer.Modify([](FStatDisplayFlags& Flags) {
                Flags.mBShowFps = !Flags.mBShowFps;
            });
        } else if (Command == "stat render") {
            Writer.Modify([](FStatDisplayFlags& Flags) {
                Flags.mBShowRender = !Flags.mBShowRender;
            });
        } else if (Command == "stat memory") {
            Writer.Modify([](FStatDisplayFlags& Flags) {
                Flags.mBShowMemory = !Flags.mBShowMemory;
            });
        } else if (Command == "stat object system") {
            Writer.Modify([](FStatDisplayFlags& Flags) {
                Flags.mBObjectSystem = !Flags.mBObjectSystem;
            });
        } else if (Command == "stat picking") {
            Writer.Modify([](FStatDisplayFlags& Flags) {
                Flags.mBShowPicking = !Flags.mBShowPicking;
            });
        } else if (Command == "stat none") {
            Writer.Modify([](FStatDisplayFlags& Flags) {
                Flags.mBShowRender = false;
            });
            Writer.Modify([](FStatDisplayFlags& Flags) {
                Flags.mBShowFps = false;
            });
            Writer.Modify([](FStatDisplayFlags& Flags) {
                Flags.mBShowMemory = false;
            });
            Writer.Modify([](FStatDisplayFlags& Flags) {
                Flags.mBObjectSystem = false;
            });
            Writer.Modify([](FStatDisplayFlags& Flags) {
                Flags.mBShowPicking = false;
            });
        } else {
            Console::AddLog(Handle, Warning, Core, "Unknown command: %s", Command.c_str());
        }
    }
}

void DrawConsoleContents(FConsoleOutputHandle Handle, FStateChannel<FStatDisplayFlags>::FWriter Writer) {
    const std::size_t Count{Console::GetMessageCount(Handle)};

    // 로그 영역
    float FooterHeight{ImGui::GetFrameHeightWithSpacing() * 2.0f};

    ImGui::BeginChild("LogRegion", ImVec2(0, -FooterHeight), true);

    Console::Flush(Handle);

    for (std::size_t Index{0}; Index < Count; ++Index) {
        const FConsoleMessage& Message{Console::GetMessageAt(Handle, Index)};

        ImGui::TextUnformatted(Message.mTime.c_str());
        ImGui::SameLine();

        const char* LevelText{"[Log]"};
        ImVec4 LevelColor{ImVec4(1.0f, 1.0f, 1.0f, 1.0f)};

        switch (Message.mLevel) {
            case ELogLevel::Warning:
                LevelText = "[Warning]";
                LevelColor = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
                break;

            case ELogLevel::Error:
                LevelText = "[Error]";
                LevelColor = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
                break;

            case ELogLevel::Fatal:
                LevelText = "[Fatal]";
                LevelColor = ImVec4(1.0f, 0.0f, 1.0f, 1.0f);
                break;

            default:
                break;
        }

        ImGui::PushStyleColor(ImGuiCol_Text, LevelColor);
        ImGui::TextUnformatted(LevelText);
        ImGui::PopStyleColor();
        ImGui::SameLine();

        ImGui::TextUnformatted(LogCategoryToString(Message.mCategory));
        ImGui::SameLine();

        ImGui::TextUnformatted(Message.mText.c_str());
    }

    ImGui::SetScrollHereY(1.0f);

    ImGui::EndChild();

    static char InputBuf[256]{""};

    ImGui::Separator();

    if (ImGui::InputText(
            "##ConsoleInput",
            InputBuf,
            sizeof(InputBuf),
            ImGuiInputTextFlags_EnterReturnsTrue)) {
        if (InputBuf[0] != '\0') {
            ExecuteCommand(Console::STDOutHandle, InputBuf, Writer);
            InputBuf[0] = '\0';

            ImGui::SetKeyboardFocusHere(-1);
        }
    }

    ImGui::TextDisabled("Type 'help' and press ENTER for available commands.");
}
