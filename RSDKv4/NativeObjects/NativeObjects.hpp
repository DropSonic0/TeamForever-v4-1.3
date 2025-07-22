#ifndef NATIVE_OBJECTS_H
#define NATIVE_OBJECTS_H

#define RSDK_THIS(type)     NativeEntity_##type *self = (NativeEntity_##type *)objPtr
#define CREATE_ENTITY(type) ((NativeEntity_##type *)CreateNativeObject(type##_Create, type##_Main))

extern bool usePhysicalControls;
extern byte timeAttackTex;
extern ushort helpText[0x1000];

#include "ps3/MenuBG.hpp"
#include "ps3/TextLabel.hpp"
#include "ps3/PushButton.hpp"
#include "ps3/SubMenuButton.hpp"
#include "ps3/DialogPanel.hpp"
#include "ps3/FadeScreen.hpp"
#include "ps3/VirtualDPad.hpp"
#include "ps3/VirtualDPadM.hpp"
#include "ps3/SettingsScreen.hpp"
#include "ps3/RetroGameLoop.hpp"
#include "ps3/PauseMenu.hpp"
#include "ps3/SegaSplash.hpp"
#include "ps3/CWSplash.hpp"
#include "ps3/TitleScreen.hpp"
#include "ps3/StartGameButton.hpp"
#include "ps3/TimeAttackButton.hpp"
#include "ps3/AchievementsButton.hpp"
#include "ps3/MultiplayerButton.hpp"
#include "ps3/LeaderboardsButton.hpp"
#if RETRO_USE_MOD_LOADER
#include "ps3/ModsButton.hpp"
#include "ps3/ModInfoButton.hpp"
#include "ps3/ModsMenu.hpp"
#endif
#include "ps3/OptionsButton.hpp"
#include "ps3/BackButton.hpp"
#include "ps3/SegaIDButton.hpp"
#include "ps3/MenuControl.hpp"
#include "ps3/SaveSelect.hpp"
#include "ps3/PlayerSelectScreen.hpp"
#include "ps3/ZoneButton.hpp"
#include "ps3/RecordsScreen.hpp"
#include "ps3/TimeAttack.hpp"
#if !RETRO_USE_ORIGINAL_CODE
#include "ps3/AchievementDisplay.hpp"
#include "ps3/AchievementsMenu.hpp"
#endif
#include "ps3/InstructionsScreen.hpp"
#include "ps3/AboutScreen.hpp"
#include "ps3/CreditText.hpp"
#include "ps3/StaffCredits.hpp"
#include "ps3/OptionsMenu.hpp"
#if RETRO_USE_NETWORKING
#include "ps3/MultiplayerHandler.hpp"
#include "ps3/MultiplayerScreen.hpp"
#endif

#endif // !NATIVE_OBJECTS_H