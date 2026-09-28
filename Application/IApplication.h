#pragma once

#include <Windows.h>

class IApplication {
public:
    virtual ~IApplication() = default;

public:
    virtual int Run(HINSTANCE Instance, int ShowCommand) = 0;
};
