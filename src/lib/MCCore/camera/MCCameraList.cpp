#include "stdafx.h"
#include "camera/MCCameraList.h"
#include "camera/MCMainWindow.h"
#include "lib/MCFitIniFile.h"
#include "logistics/logmain.h"
#include "main/MCGameContext.h"
#include "terrain/MCTerrain.h"

std::string CameraPath = "data\\cameras\\";

auto CameraList() -> MCCameraList*
{
    return MCGameContext::Current().CameraList();
}

MCCameraList::MCCameraList() = default;

MCCameraList::~MCCameraList()
{
    _Cameras.clear();
    CurrentCamera = nullptr;
    PauseShape = nullptr;
    AskedShape = nullptr;
    _MainHolder.reset();
}

auto MCCameraList::Load(std::string_view fileName) -> std::expected<void, std::string>
{
    MCFitIniFile cameraFile;

    if (const int32_t result = cameraFile.Open(GamePath(CameraPath, fileName, ".fit")); result != 0)
    {
        return std::unexpected(
            std::format("could not open the camera file {} ({:#x})", fileName, static_cast<uint32_t>(result)));
    }

    if (cameraFile.SeekBlock("CameraInfo") != 0)
    {
        return std::unexpected("no CameraInfo block in the camera file");
    }

    const MCFitResult<uint32_t> numCameras = cameraFile.Read<uint32_t>("NumCameras");

    if (!numCameras.has_value())
    {
        return std::unexpected("no NumCameras in the camera file");
    }

    if (*numCameras > MaxCameras)
    {
        return std::unexpected(std::format("{} cameras; the most is {}", *numCameras, MaxCameras));
    }

    // Cameras whose count reads as negative (2^31 or more) are none.
    for (int32_t i = 0; i < static_cast<int32_t>(*numCameras); i++)
    {
        bool objectCamera = false;

        if (cameraFile.SeekBlock(std::format("Camera{}", i)) != 0)
        {
            if (cameraFile.SeekBlock(std::format("ObjectCamera{}", i)) != 0)
            {
                return std::unexpected(std::format("no Camera{0} or ObjectCamera{0} block in the camera file", i));
            }

            objectCamera = true;
        }

        auto camera = std::make_unique<MCCamera>();

        if (std::expected<void, std::string> loaded = camera->Load(cameraFile, objectCamera, i + 1, *this); !loaded)
        {
            return loaded;
        }

        Add(std::move(camera));
    }

    return {};
}

auto MCCameraList::Add(std::unique_ptr<MCCamera> camera) -> void
{
    _Cameras.push_back(std::move(camera));
}

auto MCCameraList::EnsureMainHolder() -> std::expected<MCMainWindow*, std::string>
{
    if (_MainHolder == nullptr)
    {
        _MainHolder = MCMakeGui<MCMainWindow>();

        if (const int32_t result = _MainHolder->Init(); result != 0)
        {
            return std::unexpected(
                std::format("could not open the main window ({:#x})", static_cast<uint32_t>(result)));
        }
    }

    return _MainHolder.get();
}

auto MCCameraList::ActivateAllReady() -> MCCamera*
{
    CurrentCamera = nullptr;

    for (const std::unique_ptr<MCCamera>& camera : _Cameras)
    {
        if (camera->Ready && camera->Activate() == 0 && CurrentCamera == nullptr)
        {
            CurrentCamera = camera.get();
        }
    }

    return CurrentCamera;
}

auto MCCameraList::RenderView(MCGuiObject* window) -> void
{
    Terrain()->ClearUsedBlocks();
    MCCamera* savedEye = Eye;

    for (const std::unique_ptr<MCCamera>& camera : _Cameras)
    {
        if (camera->Active && camera->View() == window)
        {
            Eye = camera.get();
            camera->Render();
        }

        Eye = savedEye;
    }
}

auto MCCameraList::Update() -> void
{
    for (const std::unique_ptr<MCCamera>& camera : _Cameras)
    {
        if (camera->Active)
        {
            camera->Update();
        }
    }
}

auto MCCameraList::FindCameraFromIDNumber(int32_t cameraId) -> MCCamera*
{
    for (const std::unique_ptr<MCCamera>& camera : _Cameras)
    {
        if (camera->CameraId == cameraId)
        {
            return camera.get();
        }
    }

    return nullptr;
}
