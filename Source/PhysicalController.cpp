/***************************************************************************************************
 * Xidi
 *   DirectInput interface for XInput controllers.
 ***************************************************************************************************
 * Authored by Samuel Grossman
 * Copyright (c) 2016-2024
 ***********************************************************************************************//**
 * @file PhysicalController.cpp
 *   Implementation of all functionality for communicating with physical controllers.
 *
 *   NOTE: This is a modified version of the original Xidi v4.3.1 source file. The physical
 *   controller read/write backend has been converted from the Windows XInput API to SDL3's
 *   gamepad API (SDL_Gamepad). Everything not directly related to talking to
 *   the controller hardware (threading model, polling cadence, force feedback plumbing, Windows
 *   timer resolution handling, etc.) has been left unchanged.
 **************************************************************************************************/

#include "PhysicalController.h"

#include <cstdint>
#include <mutex>
#include <set>
#include <stop_token>
#include <thread>

#include <Infra/Core/Message.h>

#include <SDL3/SDL.h>

#include "ApiWindows.h"
#include "ConcurrencyWrapper.h"
#include "ControllerTypes.h"
#include "ForceFeedbackDevice.h"
#include "Globals.h"
#include "ImportApiWinMM.h"
#include "Mapper.h"
#include "Strings.h"
#include "VirtualController.h"

namespace Xidi
{
  namespace Controller
  {
    /// Raw physical state data for each of the possible physical controllers.
    static ConcurrencyWrapper<SPhysicalState> physicalControllerState[kPhysicalControllerCount];

    /// State data for each of the possible physical controllers after it is passed through a mapper
    /// but without any further processing.
    static ConcurrencyWrapper<SState> rawVirtualControllerState[kPhysicalControllerCount];

    /// Per-controller force feedback device buffer objects.
    /// These objects are not safe for dynamic initialization, so they are initialized later by
    /// pointer.
    static ForceFeedback::Device* physicalControllerForceFeedbackBuffer;

    /// Pointers to the virtual controller objects registered for force feedback with each physical
    /// controller.
    static std::set<const VirtualController*>
        physicalControllerForceFeedbackRegistration[kPhysicalControllerCount];

    /// Mutex objects for protecting against concurrent accesses to the physical controller force
    /// feedback registration data.
    static std::mutex physicalControllerForceFeedbackMutex[kPhysicalControllerCount];

    /// SDL_Gamepad handles for each of the possible physical controller slots, indexed the
    /// same way XInput controller indices were (0 through kPhysicalControllerCount - 1).
    /// Opened lazily and re-opened automatically if a device is detached and a new one shows up
    /// in the same slot.
    static SDL_Gamepad* sdlGameControllers[kPhysicalControllerCount] = {};

    /// Guards all access to the SDL joystick/gamepad subsystem. Unlike XInputGetState and
    /// XInputSetState, which Microsoft documents as safe to call concurrently from multiple
    /// threads, SDL3's joystick and gamepad APIs are not documented as thread-safe, and
    /// this file polls each controller from its own dedicated thread. All SDL_Gamepad* calls
    /// go through this mutex.
    static std::mutex sdlControllerMutex;

    /// Computes an opaque source identifier from a given controller identifier.
    /// @param [in] controllerIdentifier Identifier of the physical controller for which an
    /// identifier is needed.
    /// @return Opaque identifier that can be passed to mappers.
    static inline uint32_t OpaqueControllerSourceIdentifier(
        TControllerIdentifier controllerIdentifier)
    {
      return (uint32_t)controllerIdentifier;
    }

    /// Retrieves (opening it if necessary) the SDL_Gamepad handle associated with a
    /// physical controller slot. Must be called with #sdlControllerMutex held.
    /// @param [in] controllerIdentifier Identifier of the controller slot on which to operate.
    /// @return Open and attached game controller handle for the slot, or `nullptr` if no
    /// suitable device is currently present.
    static SDL_Gamepad* GetOpenGameControllerUnsafe(
        TControllerIdentifier controllerIdentifier)
    {
      SDL_UpdateGamepads();

      SDL_Gamepad* controller = sdlGameControllers[controllerIdentifier];

      if ((nullptr != controller) && (false == SDL_GamepadConnected(controller)))
      {
        SDL_CloseGamepad(controller);
        controller = nullptr;
        sdlGameControllers[controllerIdentifier] = nullptr;
      }

      if (nullptr == controller)
      {
        // SDL3 removes the old SDL2 device-index model (SDL_NumJoysticks() combined with
        // SDL_IsGameController() per index). Instead, SDL_GetGamepads() directly returns the
        // joystick instance IDs of devices SDL3 currently recognizes as gamepads, so no separate
        // filtering call is needed. This treats physical controller slot numbers as if they were
        // positions in that returned array, which mirrors the fixed 4-slot model XInput used.
        // That array position is not a stable identifier the way XInput's controller indices
        // were: it can shift whenever a device is added or removed elsewhere in the system. For a
        // closer match to XInput's behavior (a device keeps its slot until it disconnects), track
        // devices by instance ID using SDL_EVENT_GAMEPAD_ADDED / SDL_EVENT_GAMEPAD_REMOVED events
        // instead.
        int sdlNumGamepads = 0;
        SDL_JoystickID* const sdlGamepadInstanceIds = SDL_GetGamepads(&sdlNumGamepads);

        if (nullptr != sdlGamepadInstanceIds)
        {
          const int sdlDeviceIndex = (int)controllerIdentifier;

          if (sdlDeviceIndex < sdlNumGamepads)
          {
            controller = SDL_OpenGamepad(sdlGamepadInstanceIds[sdlDeviceIndex]);
            sdlGameControllers[controllerIdentifier] = controller;
          }

          SDL_free(sdlGamepadInstanceIds);
        }
      }

      return controller;
    }

    /// Reads physical controller state.
    /// @param [in] controllerIdentifier Identifier of the controller on which to operate.
    /// @return Physical state of the identified controller.
    static SPhysicalState ReadPhysicalControllerState(TControllerIdentifier controllerIdentifier)
    {
      // SDL_Gamepad exposes buttons individually rather than as a single register, so the
      // mask is built up below one button at a time instead of being taken directly from a
      // hardware bitfield. The first 16 bits keep the original XInput layout (dpad, start, back,
      // thumbs, shoulders, guide, misc1, A/B/X/Y). Touchpad and the four paddles occupy the
      // additional bits above that, which requires EPhysicalButton to have those enumerators and
      // the SPhysicalState button field to be at least 21 bits wide.

      std::unique_lock lock(sdlControllerMutex);

      SDL_Gamepad* const controller = GetOpenGameControllerUnsafe(controllerIdentifier);
      if (nullptr == controller) return {.deviceStatus = EPhysicalDeviceStatus::NotConnected};

      const int16_t stickLX = SDL_GetGamepadAxis(controller, SDL_GAMEPAD_AXIS_LEFTX);
      const int16_t stickLY = SDL_GetGamepadAxis(controller, SDL_GAMEPAD_AXIS_LEFTY);
      const int16_t stickRX = SDL_GetGamepadAxis(controller, SDL_GAMEPAD_AXIS_RIGHTX);
      const int16_t stickRY = SDL_GetGamepadAxis(controller, SDL_GAMEPAD_AXIS_RIGHTY);

      // SDL trigger axes report 0 to 32767 (unlike the signed -32768 to 32767 stick axes), so
      // scale down into the same 0-255 byte range XInput's bLeftTrigger/bRightTrigger used.
      const uint8_t triggerL = (uint8_t)(
          SDL_GetGamepadAxis(controller, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) >> 7);
      const uint8_t triggerR = (uint8_t)(
          SDL_GetGamepadAxis(controller, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) >> 7);

      uint32_t buttons = 0;
      if (SDL_GetGamepadButton(controller, SDL_GAMEPAD_BUTTON_DPAD_UP))
        buttons |= (1u << (unsigned int)EPhysicalButton::DpadUp);
      if (SDL_GetGamepadButton(controller, SDL_GAMEPAD_BUTTON_DPAD_DOWN))
        buttons |= (1u << (unsigned int)EPhysicalButton::DpadDown);
      if (SDL_GetGamepadButton(controller, SDL_GAMEPAD_BUTTON_DPAD_LEFT))
        buttons |= (1u << (unsigned int)EPhysicalButton::DpadLeft);
      if (SDL_GetGamepadButton(controller, SDL_GAMEPAD_BUTTON_DPAD_RIGHT))
        buttons |= (1u << (unsigned int)EPhysicalButton::DpadRight);
      if (SDL_GetGamepadButton(controller, SDL_GAMEPAD_BUTTON_START))
        buttons |= (1u << (unsigned int)EPhysicalButton::Start);
      if (SDL_GetGamepadButton(controller, SDL_GAMEPAD_BUTTON_BACK))
        buttons |= (1u << (unsigned int)EPhysicalButton::Back);
      if (SDL_GetGamepadButton(controller, SDL_GAMEPAD_BUTTON_LEFT_STICK))
        buttons |= (1u << (unsigned int)EPhysicalButton::LS);
      if (SDL_GetGamepadButton(controller, SDL_GAMEPAD_BUTTON_RIGHT_STICK))
        buttons |= (1u << (unsigned int)EPhysicalButton::RS);
      if (SDL_GetGamepadButton(controller, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER))
        buttons |= (1u << (unsigned int)EPhysicalButton::LB);
      if (SDL_GetGamepadButton(controller, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER))
        buttons |= (1u << (unsigned int)EPhysicalButton::RB);
      if (SDL_GetGamepadButton(controller, SDL_GAMEPAD_BUTTON_SOUTH))
        buttons |= (1u << (unsigned int)EPhysicalButton::A);
      if (SDL_GetGamepadButton(controller, SDL_GAMEPAD_BUTTON_EAST))
        buttons |= (1u << (unsigned int)EPhysicalButton::B);
      if (SDL_GetGamepadButton(controller, SDL_GAMEPAD_BUTTON_WEST))
        buttons |= (1u << (unsigned int)EPhysicalButton::X);
      if (SDL_GetGamepadButton(controller, SDL_GAMEPAD_BUTTON_NORTH))
        buttons |= (1u << (unsigned int)EPhysicalButton::Y);

      // Buttons that XInput never exposed but SDL3 can report, depending on the controller.
      // Guide: Xbox/PS/Switch home button. Misc1: Xbox Series share, PS5 mic mute, Switch capture.
      // Touchpad: PS4/PS5 touchpad click. Paddles: Xbox Elite, DualSense Edge, Steam Deck, etc.
      // Each one is simply absent (reads as not pressed) on controllers that lack it.
      if (SDL_GetGamepadButton(controller, SDL_GAMEPAD_BUTTON_GUIDE))
        buttons |= (1u << (unsigned int)EPhysicalButton::Guide);
      if (SDL_GetGamepadButton(controller, SDL_GAMEPAD_BUTTON_MISC1))
        buttons |= (1u << (unsigned int)EPhysicalButton::Misc1);
      if (SDL_GetGamepadButton(controller, SDL_GAMEPAD_BUTTON_TOUCHPAD))
        buttons |= (1u << (unsigned int)EPhysicalButton::Touchpad);
      if (SDL_GetGamepadButton(controller, SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1))
        buttons |= (1u << (unsigned int)EPhysicalButton::RightPaddle1);
      if (SDL_GetGamepadButton(controller, SDL_GAMEPAD_BUTTON_LEFT_PADDLE1))
        buttons |= (1u << (unsigned int)EPhysicalButton::LeftPaddle1);
      if (SDL_GetGamepadButton(controller, SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2))
        buttons |= (1u << (unsigned int)EPhysicalButton::RightPaddle2);
      if (SDL_GetGamepadButton(controller, SDL_GAMEPAD_BUTTON_LEFT_PADDLE2))
        buttons |= (1u << (unsigned int)EPhysicalButton::LeftPaddle2);

      return {
          .deviceStatus = EPhysicalDeviceStatus::Ok,
          .stick = {stickLX, stickLY, stickRX, stickRY},
          .trigger = {triggerL, triggerR},
          .button = buttons};
    }

    /// Scales a vibration strength value by the specified scaling factor. If the resulting strength
    /// exceeds the maximum possible strength it is saturated at the maximum possible strength.
    /// @param [in] vibrationStrength Physical motor vibration strength value.
    /// @param [in] scalingFactor Scaling factor by which to scale up or down the physical motor
    /// vibration strength value.
    /// @return Scaled physical motor vibration strength value that can then be sent directly to the
    /// physical motor.
    static ForceFeedback::TPhysicalActuatorValue ScaledVibrationStrength(
        ForceFeedback::TPhysicalActuatorValue vibrationStrength, double scalingFactor)
    {
      if (0.0 == scalingFactor)
        return 0;
      else if (1.0 == scalingFactor)
        return static_cast<ForceFeedback::TPhysicalActuatorValue>(vibrationStrength);

      constexpr double kMaxVibrationStrength =
          static_cast<double>(std::numeric_limits<ForceFeedback::TPhysicalActuatorValue>::max());
      const double scaledVibrationStrength = static_cast<double>(vibrationStrength) * scalingFactor;

      return static_cast<ForceFeedback::TPhysicalActuatorValue>(
          std::min(scaledVibrationStrength, kMaxVibrationStrength));
    }

    /// Writes a vibration command to a physical controller.
    /// @param [in] controllerIdentifier Identifier of the controller on which to operate.
    /// @param [in] vibration Physical actuator vibration vector.
    /// @return `true` if successful, `false` otherwise.
    static bool WritePhysicalControllerVibration(
        TControllerIdentifier controllerIdentifier,
        ForceFeedback::SPhysicalActuatorComponents vibration)
    {
      static const double kForceFeedbackEffectStrengthScalingFactor =
          static_cast<double>(
              Globals::GetConfigurationData()
                  .GetFirstIntegerValue(
                      Strings::kStrConfigurationSectionProperties,
                      Strings::kStrConfigurationSettingPropertiesForceFeedbackEffectStrengthPercent)
                  .value_or(100)) /
          100.0;

      // Impulse triggers are ignored because SDL_RumbleGamepad does not support them; a
      // controller that supports trigger-specific haptics could instead be driven via
      // SDL_RumbleGamepadTriggers.
      const Uint16 leftMotorSpeed = (Uint16)ScaledVibrationStrength(
          vibration.leftMotor, kForceFeedbackEffectStrengthScalingFactor);
      const Uint16 rightMotorSpeed = (Uint16)ScaledVibrationStrength(
          vibration.rightMotor, kForceFeedbackEffectStrengthScalingFactor);

      std::unique_lock lock(sdlControllerMutex);

      SDL_Gamepad* const controller = GetOpenGameControllerUnsafe(controllerIdentifier);
      if (nullptr == controller) return false;

      // SDL_RumbleGamepad needs an explicit duration, unlike XInputSetState which just
      // sets the motors and leaves them running until told otherwise. This is refreshed every
      // time the caller's loop notices a state change, so a duration a few periods longer than
      // the force feedback actuation period keeps the motors running smoothly between updates
      // without needing to re-issue an identical command every single cycle.
      constexpr Uint32 kRumbleDurationMilliseconds = 4 * kPhysicalForceFeedbackPeriodMilliseconds;

      // SDL_RumbleGamepad returns true on success and false on failure in SDL3 (SDL2's
      // equivalent function returned 0 on success).
      return SDL_RumbleGamepad(
          controller, leftMotorSpeed, rightMotorSpeed, kRumbleDurationMilliseconds);
    }

    /// Periodically plays force feedback effects on the physical controller actuators.
    /// @param [in] controllerIdentifier Identifier of the controller on which to operate.
    static void ForceFeedbackActuateEffects(TControllerIdentifier controllerIdentifier)
    {
      constexpr ForceFeedback::TOrderedMagnitudeComponents kVirtualMagnitudeVectorZero = {};

      ForceFeedback::SPhysicalActuatorComponents previousPhysicalActuatorValues;
      ForceFeedback::SPhysicalActuatorComponents currentPhysicalActuatorValues;

      const Mapper* mapper = Mapper::GetConfigured(controllerIdentifier);
      bool lastActuationResult = true;

      while (true)
      {
        if (true == lastActuationResult)
          Sleep(kPhysicalForceFeedbackPeriodMilliseconds);
        else
          Sleep(kPhysicalErrorBackoffPeriodMilliseconds);

        if (true == Globals::DoesCurrentProcessHaveInputFocus())
        {
          ForceFeedback::TEffectValue overallEffectGain = 10000;
          ForceFeedback::SPhysicalActuatorComponents physicalActuatorVector = {};
          ForceFeedback::TOrderedMagnitudeComponents virtualMagnitudeVector =
              physicalControllerForceFeedbackBuffer[controllerIdentifier].PlayEffects();

          if (kVirtualMagnitudeVectorZero != virtualMagnitudeVector)
          {
            std::unique_lock lock(physicalControllerForceFeedbackMutex[controllerIdentifier]);

            // Gain is modified downwards by each virtual controller object.
            // Typically there would only be one, in which case the properties of that object would
            // be effective. Otherwise this loop is essentially modeled as multiple volume knobs
            // connected in sequence, each lowering the volume of the effects by the value of its
            // own device-wide gain property.
            for (auto virtualController :
                 physicalControllerForceFeedbackRegistration[controllerIdentifier])
              overallEffectGain *=
                  ((ForceFeedback::TEffectValue)virtualController->GetForceFeedbackGain() /
                   ForceFeedback::kEffectModifierMaximum);

            physicalActuatorVector = mapper->MapForceFeedbackVirtualToPhysical(
                virtualMagnitudeVector, overallEffectGain);
          }

          currentPhysicalActuatorValues = physicalActuatorVector;
        }
        else
        {
          currentPhysicalActuatorValues = {};
        }

        if (previousPhysicalActuatorValues != currentPhysicalActuatorValues)
        {
          lastActuationResult =
              WritePhysicalControllerVibration(controllerIdentifier, currentPhysicalActuatorValues);
          previousPhysicalActuatorValues = currentPhysicalActuatorValues;
        }
        else
        {
          lastActuationResult = true;
        }
      }
    }

    /// Periodically polls for physical controller state.
    /// On detected state change, updates the internal data structure and notifies all waiting
    /// threads.
    /// @param [in] controllerIdentifier Identifier of the controller on which to operate.
    static void PollForPhysicalControllerStateChanges(TControllerIdentifier controllerIdentifier)
    {
      SPhysicalState newPhysicalState = physicalControllerState[controllerIdentifier].Get();

      while (true)
      {
        if (EPhysicalDeviceStatus::Ok == newPhysicalState.deviceStatus)
          Sleep(kPhysicalPollingPeriodMilliseconds);
        else
          Sleep(kPhysicalErrorBackoffPeriodMilliseconds);

        newPhysicalState = ReadPhysicalControllerState(controllerIdentifier);

        if (true == physicalControllerState[controllerIdentifier].Update(newPhysicalState))
        {
          const SState newRawVirtualState =
              ((EPhysicalDeviceStatus::Ok == newPhysicalState.deviceStatus)
                   ? Mapper::GetConfigured(controllerIdentifier)
                         ->MapStatePhysicalToVirtual(
                             newPhysicalState,
                             OpaqueControllerSourceIdentifier(controllerIdentifier))
                   : Mapper::GetConfigured(controllerIdentifier)
                         ->MapNeutralPhysicalToVirtual(
                             OpaqueControllerSourceIdentifier(controllerIdentifier)));

          rawVirtualControllerState[controllerIdentifier].Update(newRawVirtualState);
        }
      }
    }

    /// Monitors physical controller status for events like hardware connection or disconnection and
    /// error conditions. Used exclusively for logging. Intended to be a thread entry point, one
    /// thread per monitored physical controller.
    /// @param [in] controllerIdentifier Identifier of the controller to monitor.
    static void MonitorPhysicalControllerStatus(TControllerIdentifier controllerIdentifier)
    {
      if (controllerIdentifier >= kPhysicalControllerCount)
      {
        Infra::Message::OutputFormatted(
            Infra::Message::ESeverity::Error,
            L"Attempted to monitor physical controller with invalid identifier %u.",
            controllerIdentifier);
        return;
      }

      SPhysicalState oldPhysicalState = GetCurrentPhysicalControllerState(controllerIdentifier);
      SPhysicalState newPhysicalState = oldPhysicalState;

      while (true)
      {
        WaitForPhysicalControllerStateChange(
            controllerIdentifier, newPhysicalState, std::stop_token());

        // Look for status changes and output to the log, as appropriate.
        switch (newPhysicalState.deviceStatus)
        {
          case EPhysicalDeviceStatus::Ok:
            switch (oldPhysicalState.deviceStatus)
            {
              case EPhysicalDeviceStatus::Ok:
                break;

              case EPhysicalDeviceStatus::NotConnected:
                Infra::Message::OutputFormatted(
                    Infra::Message::ESeverity::Info,
                    L"Physical controller %u: Hardware connected.",
                    (1 + controllerIdentifier));
                break;

              default:
                Infra::Message::OutputFormatted(
                    Infra::Message::ESeverity::Warning,
                    L"Physical controller %u: Cleared previous error condition.",
                    (1 + controllerIdentifier));
                break;
            }
            break;

          case EPhysicalDeviceStatus::NotConnected:
            if (newPhysicalState.deviceStatus != oldPhysicalState.deviceStatus)
              Infra::Message::OutputFormatted(
                  Infra::Message::ESeverity::Info,
                  L"Physical controller %u: Hardware disconnected.",
                  (1 + controllerIdentifier));
            break;

          default:
            if (newPhysicalState.deviceStatus != oldPhysicalState.deviceStatus)
              Infra::Message::OutputFormatted(
                  Infra::Message::ESeverity::Warning,
                  L"Physical controller %u: Encountered an error condition.",
                  (1 + controllerIdentifier));
            break;
        }

        oldPhysicalState = newPhysicalState;
      }
    }

    /// Initializes internal data structures and creates worker threads.
    /// Idempotent and concurrency-safe.
    static void Initialize(void)
    {
      static std::once_flag initFlag;
      std::call_once(
          initFlag,
          []() -> void
          {
            // Initialize SDL's gamepad subsystem (this transitively initializes the
            // joystick subsystem as well, so no separate SDL_INIT_JOYSTICK call is needed).
            // This replaces the implicit initialization that used to happen the first time
            // ImportApiXInput resolved the XInput DLL's entry points.
            {
              std::unique_lock lock(sdlControllerMutex);

              // SDL_InitSubSystem returns true on success and false on failure in SDL3 (SDL2's
              // equivalent function returned 0 on success).
              if (false == SDL_InitSubSystem(SDL_INIT_GAMEPAD))
              {
                Infra::Message::OutputFormatted(
                    Infra::Message::ESeverity::Error,
                    L"Failed to initialize the SDL gamepad subsystem: %S",
                    SDL_GetError());
              }
              else
              {
                // Optional community mapping database; must be loaded before any controller is
                // opened.
                SDL_AddGamepadMappingsFromFile("gamecontrollerdb.txt");
              }
            }

            // Initialize controller state data structures.
            for (auto controllerIdentifier = 0;
                 controllerIdentifier < _countof(physicalControllerState);
                 ++controllerIdentifier)
            {
              const SPhysicalState initialPhysicalState =
                  ReadPhysicalControllerState(controllerIdentifier);
              const SState initialRawVirtualState =
                  Mapper::GetConfigured(controllerIdentifier)
                      ->MapStatePhysicalToVirtual(
                          initialPhysicalState,
                          OpaqueControllerSourceIdentifier(controllerIdentifier));

              physicalControllerState[controllerIdentifier].Set(initialPhysicalState);
              rawVirtualControllerState[controllerIdentifier].Set(initialRawVirtualState);
            }

            // Ensure the system timer resolution is suitable for the desired polling frequency.
            TIMECAPS timeCaps;
            MMRESULT timeResult = ImportApiWinMM::timeGetDevCaps(&timeCaps, sizeof(timeCaps));
            if (MMSYSERR_NOERROR == timeResult)
            {
              timeResult = ImportApiWinMM::timeBeginPeriod(timeCaps.wPeriodMin);

              if (MMSYSERR_NOERROR == timeResult)
                Infra::Message::OutputFormatted(
                    Infra::Message::ESeverity::Info,
                    L"Set the system timer resolution to %u ms.",
                    timeCaps.wPeriodMin);
              else
                Infra::Message::OutputFormatted(
                    Infra::Message::ESeverity::Warning,
                    L"Failed with code %u to set the system timer resolution.",
                    timeResult);
            }
            else
            {
              Infra::Message::OutputFormatted(
                  Infra::Message::ESeverity::Warning,
                  L"Failed with code %u to obtain system timer resolution information.",
                  timeResult);
            }

            // Create and start the polling threads.
            for (auto controllerIdentifier = 0; controllerIdentifier < kPhysicalControllerCount;
                 ++controllerIdentifier)
            {
              std::thread(PollForPhysicalControllerStateChanges, controllerIdentifier).detach();
              Infra::Message::OutputFormatted(
                  Infra::Message::ESeverity::Info,
                  L"Initialized the physical controller state polling thread for controller %u. Desired polling period is %u ms.",
                  (unsigned int)(1 + controllerIdentifier),
                  kPhysicalPollingPeriodMilliseconds);
            }

            // Allocate the force feedback device buffers, then create and start the force feedback
            // threads.
            physicalControllerForceFeedbackBuffer =
                new ForceFeedback::Device[kPhysicalControllerCount];
            for (auto controllerIdentifier = 0; controllerIdentifier < kPhysicalControllerCount;
                 ++controllerIdentifier)
            {
              std::thread(ForceFeedbackActuateEffects, controllerIdentifier).detach();
              Infra::Message::OutputFormatted(
                  Infra::Message::ESeverity::Info,
                  L"Initialized the physical controller force feedback actuation thread for controller %u. Desired actuation period is %u ms.",
                  (unsigned int)(1 + controllerIdentifier),
                  kPhysicalForceFeedbackPeriodMilliseconds);
            }

            // Create and start the physical controller hardware status monitoring threads, but only
            // if the messages generated by those threads will actually be delivered as output.
            if (Infra::Message::WillOutputMessageOfSeverity(Infra::Message::ESeverity::Warning))
            {
              for (auto controllerIdentifier = 0; controllerIdentifier < kPhysicalControllerCount;
                   ++controllerIdentifier)
              {
                std::thread(MonitorPhysicalControllerStatus, controllerIdentifier).detach();
                Infra::Message::OutputFormatted(
                    Infra::Message::ESeverity::Info,
                    L"Initialized the physical controller hardware status monitoring thread for controller %u.",
                    (unsigned int)(1 + controllerIdentifier));
              }
            }
          });
    }

    SCapabilities GetControllerCapabilities(TControllerIdentifier controllerIdentifier)
    {
      Initialize();
      return Mapper::GetConfigured(controllerIdentifier)->GetCapabilities();
    }

    SPhysicalState GetCurrentPhysicalControllerState(TControllerIdentifier controllerIdentifier)
    {
      Initialize();
      return physicalControllerState[controllerIdentifier].Get();
    }

    SState GetCurrentRawVirtualControllerState(TControllerIdentifier controllerIdentifier)
    {
      Initialize();
      return rawVirtualControllerState[controllerIdentifier].Get();
    }

    ForceFeedback::Device* PhysicalControllerForceFeedbackRegister(
        TControllerIdentifier controllerIdentifier, const VirtualController* virtualController)
    {
      Initialize();

      if (controllerIdentifier >= kPhysicalControllerCount)
      {
        Infra::Message::OutputFormatted(
            Infra::Message::ESeverity::Error,
            L"Attempted to register with a physical controller for force feedback with invalid identifier %u.",
            controllerIdentifier);
        return nullptr;
      }

      std::unique_lock lock(physicalControllerForceFeedbackMutex[controllerIdentifier]);
      physicalControllerForceFeedbackRegistration[controllerIdentifier].insert(virtualController);

      return &physicalControllerForceFeedbackBuffer[controllerIdentifier];
    }

    void PhysicalControllerForceFeedbackUnregister(
        TControllerIdentifier controllerIdentifier, const VirtualController* virtualController)
    {
      Initialize();

      if (controllerIdentifier >= kPhysicalControllerCount)
      {
        Infra::Message::OutputFormatted(
            Infra::Message::ESeverity::Error,
            L"Attempted to unregister with a physical controller for force feedback with invalid identifier %u.",
            controllerIdentifier);
        return;
      }

      std::unique_lock lock(physicalControllerForceFeedbackMutex[controllerIdentifier]);
      physicalControllerForceFeedbackRegistration[controllerIdentifier].erase(virtualController);
    }

    bool WaitForPhysicalControllerStateChange(
        TControllerIdentifier controllerIdentifier,
        SPhysicalState& state,
        std::stop_token stopToken)
    {
      Initialize();

      if (controllerIdentifier >= kPhysicalControllerCount) return false;

      return physicalControllerState[controllerIdentifier].WaitForUpdate(state, stopToken);
    }

    bool WaitForRawVirtualControllerStateChange(
        TControllerIdentifier controllerIdentifier, SState& state, std::stop_token stopToken)
    {
      Initialize();

      if (controllerIdentifier >= kPhysicalControllerCount) return false;

      return rawVirtualControllerState[controllerIdentifier].WaitForUpdate(state, stopToken);
    }
  } // namespace Controller
} // namespace Xidi
