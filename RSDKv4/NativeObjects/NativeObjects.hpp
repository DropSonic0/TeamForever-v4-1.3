#ifndef NATIVE_OBJECTS_H
#define NATIVE_OBJECTS_H

#define RSDK_THIS(type)     NativeEntity_##type *self = (NativeEntity_##type *)objPtr
#define CREATE_ENTITY(type) ((NativeEntity_##type *)CreateNativeObject(type##_Create, type##_Main))

extern bool usePhysicalControls;
extern byte timeAttackTex;
extern ushort helpText[0x1000];

#include "new/MenuBG.hpp"
#include "new/TextLabel.hpp"
#include "new/PushButton.hpp"
#include "new/SubMenuButton.hpp"
#include "new/DialogPanel.hpp"
#include "new/FadeScreen.hpp"
#include "new/VirtualDPad.hpp"
#include "new/VirtualDPadM.hpp"
#include "new/SettingsScreen.hpp"
#include "new/RetroGameLoop.hpp"
#include "new/PauseMenu.hpp"
#include "new/SegaSplash.hpp"
#include "new/CWSplash.hpp"
#include "new/TitleScreen.hpp"
#include "new/StartGameButton.hpp"
#include "new/TimeAttackButton.hpp"
#include "new/AchievementsButton.hpp"
#include "new/MultiplayerButton.hpp"
#include "new/LeaderboardsButton.hpp"
#if RETRO_USE_MOD_LOADER
#include "new/ModsButton.hpp"
#include "new/ModInfoButton.hpp"
#include "new/ModsMenu.hpp"
#endif
#include "new/OptionsButton.hpp"
#include "new/BackButton.hpp"
#include "new/SegaIDButton.hpp"
#include "new/MenuControl.hpp"
#include "new/SaveSelect.hpp"
#include "new/PlayerSelectScreen.hpp"
#include "new/ZoneButton.hpp"
#include "new/RecordsScreen.hpp"
#include "new/TimeAttack.hpp"
#if !RETRO_USE_ORIGINAL_CODE
#include "new/AchievementDisplay.hpp"
#include "new/AchievementsMenu.hpp"
#endif
#include "new/InstructionsScreen.hpp"
#include "new/AboutScreen.hpp"
#include "new/CreditText.hpp"
#include "new/StaffCredits.hpp"
#include "new/OptionsMenu.hpp"
#if RETRO_USE_NETWORKING
#include "new/MultiplayerHandler.hpp"
#include "new/MultiplayerScreen.hpp"
#endif

#endif // !NATIVE_OBJECTS_H