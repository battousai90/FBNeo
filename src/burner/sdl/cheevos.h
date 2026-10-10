// RetroAchievements for the SDL build (rcheevos rc_client).
//
// Off unless the frontend (Bootcade) hands over an account in the
// environment : BOOTCADE_RA_USER and BOOTCADE_RA_TOKEN, plus
// BOOTCADE_RA_HARDCORE ("1" for hardcore, softcore otherwise).
//
// The memory is exposed exactly as the libretro core does it
// (src/burner/libretro/retro_memory.cpp in libretro/FBNeo) : the existing
// Arcade sets were made on that core, and their addresses only mean
// something with the same layout.
#ifndef _CHEEVOS_H_
#define _CHEEVOS_H_

void CheevosInit();          // after DrvInit, before the first frame
void CheevosExit();          // before DrvExit
void CheevosFrame();         // after every emulated frame
void CheevosIdle();          // while paused
void CheevosReset();         // the game was reset

// Hardcore mode is on : save states, slow motion and cheats are refused.
bool CheevosHardcore();
// Shows why a feature is refused in hardcore mode ; returns true when it is.
bool CheevosRefuse(const char* what);

#endif
