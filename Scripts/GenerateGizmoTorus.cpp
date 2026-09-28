#include "pch.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numbers>
#include <vector>

namespace {
    constexpr std::uint32_t MagicNumber{0x4d45534d};
    constexpr std::uint32_t Version{1};
    constexpr std::uint32_t MajorSegments{32};
    constexpr std::uint32_t MinorSegments{16};
    constexpr float MajorRadius{0.49f};
    constexpr float MinorRadius{0.01f};

    struct FVector3 {
        float mX{};
        float mY{};
        float mZ{};
    };

    struct FVector2 {
        float mX{};
        float mY{};
    };

    static_assert(sizeof(FVector3) == sizeof(float) * 3);
    static_assert(sizeof(FVector2) == sizeof(float) * 2);

    template <typename T> void WriteValue(std::ofstream& Output, const T& Value) {
        Output.write(reinterpret_cast<const char*>(&Value), sizeof(Value));
    }

    template <typename T> void WriteArray(std::ofstream& Output, const std::vector<T>& Values) {
        const auto Count{static_cast<std::uint32_t>(Values.size())};
        WriteValue(Output, Count);
        Output.write(reinterpret_cast<const char*>(Values.data()), sizeof(T) * Values.size());
    }

    bool Generate(const std::filesystem::path& OutputPath) {
        std::vector<FVector3> Positions{};
        std::vector<FVector3> Normals{};
        std::vector<FVector2> TexCoords{};
        std::vector<std::uint32_t> Indices{};

        Positions.reserve((MajorSegments + 1) * (MinorSegments + 1));
        Normals.reserve(Positions.capacity());
        TexCoords.reserve(Positions.capacity());
        Indices.reserve(MajorSegments * MinorSegments * 6);

        for (std::uint32_t Major{0}; Major <= MajorSegments; ++Major) {
            const float U{static_cast<float>(Major) / static_cast<float>(MajorSegments)};
            const float Theta{U * std::numbers::pi_v<float> * 2.0f};
            const float CosTheta{std::cos(Theta)};
            const float SinTheta{std::sin(Theta)};

            for (std::uint32_t Minor{0}; Minor <= MinorSegments; ++Minor) {
                const float V{static_cast<float>(Minor) / static_cast<float>(MinorSegments)};
                const float Phi{V * std::numbers::pi_v<float> * 2.0f};
                const float CosPhi{std::cos(Phi)};
                const float SinPhi{std::sin(Phi)};
                const float RingRadius{MajorRadius + MinorRadius * CosPhi};

                Positions.push_back({RingRadius * CosTheta, -RingRadius * SinTheta, MinorRadius * SinPhi});
                Normals.push_back({CosPhi * CosTheta, -CosPhi * SinTheta, SinPhi});
                TexCoords.push_back({U, V});
            }
        }

        for (std::uint32_t Major{0}; Major < MajorSegments; ++Major) {
            for (std::uint32_t Minor{0}; Minor < MinorSegments; ++Minor) {
                const std::uint32_t Current{Major * (MinorSegments + 1) + Minor};
                const std::uint32_t MinorNext{Current + 1};
                const std::uint32_t MajorNext{(Major + 1) * (MinorSegments + 1) + Minor};
                const std::uint32_t MajorMinorNext{MajorNext + 1};

                Indices.push_back(Current);
                Indices.push_back(MinorNext);
                Indices.push_back(MajorMinorNext);
                Indices.push_back(Current);
                Indices.push_back(MajorMinorNext);
                Indices.push_back(MajorNext);
            }
        }

        std::ofstream Output{OutputPath, std::ios::binary | std::ios::trunc};
        if (!Output) {
            return false;
        }

        WriteValue(Output, MagicNumber);
        WriteValue(Output, Version);
        WriteArray(Output, Positions);
        WriteArray(Output, Normals);
        WriteArray(Output, TexCoords);
        WriteArray(Output, Indices);

        const std::uint32_t EmptyStringLength{0};
        const std::uint32_t EmptyArrayCount{0};
        WriteValue(Output, EmptyStringLength);
        WriteValue(Output, EmptyArrayCount);
        WriteValue(Output, EmptyArrayCount);

        return static_cast<bool>(Output);
    }
}

int main(int ArgumentCount, char** Arguments) {
    if (ArgumentCount != 2) {
        std::cerr << "Usage: GenerateGizmoTorus <output.bin>\n";
        return 1;
    }

    const std::filesystem::path OutputPath{Arguments[1]};
    if (!Generate(OutputPath)) {
        std::cerr << "Failed to generate " << OutputPath << '\n';
        return 1;
    }

    std::cout << "Generated " << OutputPath << '\n';
    return 0;
}
