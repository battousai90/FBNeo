// RetroAchievements for the SDL build : see cheevos.h.
#include "burner.h"
#include "cheevos.h"

#include "rc_client.h"
#include "rc_consoles.h"
#include "rc_libretro.h"

#include <curl/curl.h>

#include <atomic>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// ── Memory, as the libretro core exposes it ────────────────────────────────
//
// StateGetMainRamAcb is copied from libretro/FBNeo
// (src/burner/libretro/retro_memory.cpp) without a change in its logic : the
// Arcade achievement sets were written against that layout. Keep it in step
// with upstream when it changes.
static void* pMainRamData = NULL;
static size_t nMainRamSize = 0;
static bool bMainRamFound = false;
static int nMemoryCount = 0;
static struct retro_memory_descriptor sMemoryDescriptors[10] = {};
static bool bMemoryMapFound = false;

static int StateGetMainRamAcb(BurnArea *pba)
{
	if(!pba->szName)
		return 0;

	switch (BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK)
	{
		case HARDWARE_CAPCOM_CPS1:
		case HARDWARE_CAPCOM_CPS1_QSOUND:
		case HARDWARE_CAPCOM_CPS1_GENERIC:
		case HARDWARE_CAPCOM_CPSCHANGER:
		case HARDWARE_CAPCOM_CPS2:
			if (strcmp(pba->szName, "CpsRamFF") == 0) {
				pMainRamData = pba->Data;
				nMainRamSize = pba->nLen;
				bMainRamFound = true;
			}
			return 0;
		case HARDWARE_CAPCOM_CPS3:
			if (strcmp(pba->szName, "Main RAM") == 0) {
				pMainRamData = pba->Data;
				nMainRamSize = pba->nLen;
				bMainRamFound = true;
			}
			return 0;
		case HARDWARE_SNK_NEOCD:
			if ((strcmp(pba->szName, "68K program RAM") == 0)) {
				sMemoryDescriptors[nMemoryCount].flags     = RETRO_MEMDESC_SYSTEM_RAM;
				sMemoryDescriptors[nMemoryCount].ptr       = pba->Data;
				sMemoryDescriptors[nMemoryCount].start     = 0x00000000;
				sMemoryDescriptors[nMemoryCount].len       = pba->nLen;
				sMemoryDescriptors[nMemoryCount].select    = 0;
				sMemoryDescriptors[nMemoryCount].addrspace = pba->szName;
				bMemoryMapFound = true;
				nMemoryCount++;
			}
			if ((strcmp(pba->szName, "Memory card") == 0)) {
				sMemoryDescriptors[nMemoryCount].flags     = RETRO_MEMDESC_SAVE_RAM;
				sMemoryDescriptors[nMemoryCount].ptr       = pba->Data;
				sMemoryDescriptors[nMemoryCount].start     = 0x00800000;
				sMemoryDescriptors[nMemoryCount].len       = pba->nLen;
				sMemoryDescriptors[nMemoryCount].select    = 0;
				sMemoryDescriptors[nMemoryCount].addrspace = pba->szName;
				bMemoryMapFound = true;
				nMemoryCount++;
			}
			return 0;
		case HARDWARE_SNK_NEOGEO:
		case HARDWARE_IGS_PGM:
			if (strcmp(pba->szName, "68K RAM") == 0) {
				pMainRamData = pba->Data;
				nMainRamSize = pba->nLen;
				bMainRamFound = true;
			}
			return 0;
		case HARDWARE_PSIKYO:
			// Psikyo (psikyosh and psikyo4 uses "All RAM")
			if ((strcmp(pba->szName, "All RAM") == 0) || (strcmp(pba->szName, "68K RAM") == 0)) {
				pMainRamData = pba->Data;
				nMainRamSize = pba->nLen;
				bMainRamFound = true;
			}
			return 0;
		case HARDWARE_CAVE_68K_Z80:
		case HARDWARE_CAVE_68K_ONLY:
			// Cave (gaia driver uses "68K RAM")
			if ((strcmp(pba->szName, "RAM") == 0) || (strcmp(pba->szName, "68K RAM") == 0)) {
				pMainRamData = pba->Data;
				nMainRamSize = pba->nLen;
				bMainRamFound = true;
			}
			return 0;
		case HARDWARE_SEGA_MEGADRIVE:
			if ((strcmp(pba->szName, "RAM") == 0)) {
				sMemoryDescriptors[nMemoryCount].flags     = RETRO_MEMDESC_SYSTEM_RAM;
				sMemoryDescriptors[nMemoryCount].ptr       = pba->Data;
				sMemoryDescriptors[nMemoryCount].start     = 0x00FF0000;
				sMemoryDescriptors[nMemoryCount].len       = pba->nLen;
				sMemoryDescriptors[nMemoryCount].select    = 0;
				sMemoryDescriptors[nMemoryCount].addrspace = pba->szName;
				bMemoryMapFound = true;
				nMemoryCount++;
			}
			if ((strcmp(pba->szName, "NV RAM") == 0)) {
				sMemoryDescriptors[nMemoryCount].flags     = RETRO_MEMDESC_SAVE_RAM;
				sMemoryDescriptors[nMemoryCount].ptr       = pba->Data;
				sMemoryDescriptors[nMemoryCount].start     = 0x00000000;
				sMemoryDescriptors[nMemoryCount].len       = pba->nLen;
				sMemoryDescriptors[nMemoryCount].select    = 0;
				sMemoryDescriptors[nMemoryCount].addrspace = pba->szName;
				bMemoryMapFound = true;
				nMemoryCount++;
			}
			return 0;
		case HARDWARE_SEGA_MASTER_SYSTEM:
		case HARDWARE_SEGA_GAME_GEAR:
			if ((strcmp(pba->szName, "sms") == 0)) {
				pMainRamData = pba->Data;
				nMainRamSize = pba->nLen;
				bMainRamFound = true;
			}
			return 0;
		case HARDWARE_NES:
		case HARDWARE_FDS:
		case HARDWARE_NVS:
			if ((strcmp(pba->szName, "CPU Ram") == 0)) {
				sMemoryDescriptors[nMemoryCount].flags     = RETRO_MEMDESC_SYSTEM_RAM;
				sMemoryDescriptors[nMemoryCount].ptr       = pba->Data;
				sMemoryDescriptors[nMemoryCount].start     = 0x00000000;
				sMemoryDescriptors[nMemoryCount].len       = pba->nLen;
				sMemoryDescriptors[nMemoryCount].select    = 0;
				sMemoryDescriptors[nMemoryCount].addrspace = pba->szName;
				bMemoryMapFound = true;
				nMemoryCount++;
			}
			if ((strcmp(pba->szName, "Work Ram") == 0)) {
				sMemoryDescriptors[nMemoryCount].flags     = RETRO_MEMDESC_SYSTEM_RAM;
				sMemoryDescriptors[nMemoryCount].ptr       = pba->Data;
				sMemoryDescriptors[nMemoryCount].start     = 0x00006000;
				sMemoryDescriptors[nMemoryCount].len       = pba->nLen;
				sMemoryDescriptors[nMemoryCount].select    = 0;
				sMemoryDescriptors[nMemoryCount].addrspace = pba->szName;
				bMemoryMapFound = true;
				nMemoryCount++;
			}
			return 0;
		case HARDWARE_SNK_NGP:
		case HARDWARE_SNK_NGPC:
			if ((strcmp(pba->szName, "Main Ram") == 0)) {
				sMemoryDescriptors[nMemoryCount].flags     = RETRO_MEMDESC_SYSTEM_RAM;
				sMemoryDescriptors[nMemoryCount].ptr       = pba->Data;
				sMemoryDescriptors[nMemoryCount].start     = 0x00004000;
				sMemoryDescriptors[nMemoryCount].len       = pba->nLen;
				sMemoryDescriptors[nMemoryCount].select    = 0;
				sMemoryDescriptors[nMemoryCount].addrspace = pba->szName;
				bMemoryMapFound = true;
				nMemoryCount++;
			}
			if ((strcmp(pba->szName, "Shared Ram") == 0)) {
				sMemoryDescriptors[nMemoryCount].flags     = RETRO_MEMDESC_SYSTEM_RAM;
				sMemoryDescriptors[nMemoryCount].ptr       = pba->Data;
				sMemoryDescriptors[nMemoryCount].start     = 0x00007000;
				sMemoryDescriptors[nMemoryCount].len       = pba->nLen;
				sMemoryDescriptors[nMemoryCount].select    = 0;
				sMemoryDescriptors[nMemoryCount].addrspace = pba->szName;
				bMemoryMapFound = true;
				nMemoryCount++;
			}
			return 0;
		case HARDWARE_TOAPLAN_RAIZING:
			if ((strcmp(pba->szName, "All Ram") == 0) || (strcmp(pba->szName, "All RAM") == 0) || (strcmp(pba->szName, "RAM") == 0)) {
				pMainRamData = pba->Data;
				nMainRamSize = pba->nLen;
				bMainRamFound = true;
			}
			return 0;
		default:
			// For all other systems (?), main ram seems to be identified by either "All Ram" or "All RAM"
			if ((strcmp(pba->szName, "All Ram") == 0) || (strcmp(pba->szName, "All RAM") == 0)) {
				pMainRamData = pba->Data;
				nMainRamSize = pba->nLen;
				bMainRamFound = true;
			}
			return 0;
	}
}


static rc_libretro_memory_regions_t sRegions;

static void RC_CCONV GetCoreMemoryInfo(uint32_t id, rc_libretro_core_memory_info_t* info)
{
	if (id == RETRO_MEMORY_SYSTEM_RAM) {
		info->data = (uint8_t*)pMainRamData;
		info->size = nMainRamSize;
	} else {
		info->data = NULL;
		info->size = 0;
	}
}

static uint32_t RC_CCONV ReadMemory(uint32_t address, uint8_t* buffer, uint32_t num_bytes, rc_client_t*)
{
	return rc_libretro_memory_read(&sRegions, address, buffer, num_bytes);
}

static void MapMemory()
{
	INT32 nMin = 0;
	pMainRamData = NULL;
	nMainRamSize = 0;
	bMainRamFound = false;
	nMemoryCount = 0;
	memset(sMemoryDescriptors, 0, sizeof(sMemoryDescriptors));
	bMemoryMapFound = false;

	INT32 (__cdecl *pOldAcb)(struct BurnArea*) = BurnAcb;
	BurnAcb = StateGetMainRamAcb;
	BurnAreaScan(ACB_FULLSCAN, &nMin);
	BurnAcb = pOldAcb;

	struct retro_memory_map sMap = {};
	sMap.descriptors = sMemoryDescriptors;
	sMap.num_descriptors = nMemoryCount;
	rc_libretro_memory_init(&sRegions, bMemoryMapFound ? &sMap : NULL, GetCoreMemoryInfo, RC_CONSOLE_ARCADE);
	printf("[Cheevos] %u memory region(s), %u bytes\n", (unsigned)sRegions.count, (unsigned)sRegions.total_size);
}

// ── Messages ───────────────────────────────────────────────────────────────
//
// Server answers arrive on other threads ; the screen message is only
// written from the emulation thread, in CheevosFrame.
static std::mutex sMessageMutex;
static std::deque<std::string> sMessages;

static void Say(const std::string& text)
{
	printf("[Cheevos] %s\n", text.c_str());
	std::lock_guard<std::mutex> lock(sMessageMutex);
	sMessages.push_back(text);
}

static void FlushMessages()
{
	// One message at a time, each for its full duration.
	if (messageFrames > 1) return;
	std::string text;
	{
		std::lock_guard<std::mutex> lock(sMessageMutex);
		if (sMessages.empty()) return;
		text = sMessages.front();
		sMessages.pop_front();
	}
	char buffer[MESSAGE_MAX_LENGTH];
	snprintf(buffer, sizeof(buffer), "%s", text.c_str());
	UpdateMessage(buffer);
}

// ── Server calls, over libcurl ─────────────────────────────────────────────
static std::atomic<bool> bStopping(false);
static std::mutex sThreadsMutex;
static std::vector<std::thread> sThreads;
static std::string sUserAgent;

static size_t WriteBody(char* p, size_t size, size_t n, void* out)
{
	static_cast<std::string*>(out)->append(p, size * n);
	return size * n;
}

static int AbortWhenStopping(void*, curl_off_t, curl_off_t, curl_off_t, curl_off_t)
{
	return bStopping.load() ? 1 : 0;
}

static void RC_CCONV ServerCall(const rc_api_request_t* request, rc_client_server_callback_t callback,
                                void* callback_data, rc_client_t*)
{
	std::string url = request->url ? request->url : "";
	std::string post = request->post_data ? request->post_data : "";
	std::string type = request->content_type ? request->content_type : "";
	std::lock_guard<std::mutex> lock(sThreadsMutex);
	sThreads.emplace_back([url, post, type, callback, callback_data]() {
		std::string body;
		long status = 0;
		CURL* curl = curl_easy_init();
		CURLcode res = CURLE_FAILED_INIT;
		if (curl) {
			struct curl_slist* headers = NULL;
			if (!type.empty()) headers = curl_slist_append(headers, ("Content-Type: " + type).c_str());
			curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
			if (!post.empty()) curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post.c_str());
			if (headers) curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
			curl_easy_setopt(curl, CURLOPT_USERAGENT, sUserAgent.c_str());
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteBody);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
			curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
			curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
			curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
			curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
			curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, AbortWhenStopping);
			res = curl_easy_perform(curl);
			curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
			curl_slist_free_all(headers);
			curl_easy_cleanup(curl);
		}
		if (bStopping.load()) return;   // the client is going away : nobody to answer
		rc_api_server_response_t response;
		memset(&response, 0, sizeof(response));
		response.body = body.c_str();
		response.body_length = body.size();
		response.http_status_code = (res == CURLE_OK) ? (int)status : RC_API_SERVER_RESPONSE_RETRYABLE_CLIENT_ERROR;
		callback(&response, callback_data);
	});
}

// ── The client ─────────────────────────────────────────────────────────────
extern bool do_reset_game;   // inp_sdl2.cpp : presses F3 (reset) on the next input poll

static rc_client_t* pClient = NULL;
static bool bHardcore = false;

static void RC_CCONV LogMessage(const char* message, const rc_client_t*)
{
	printf("[Cheevos] %s\n", message);
}

static void RC_CCONV OnEvent(const rc_client_event_t* event, rc_client_t* client)
{
	char text[256];
	switch (event->type) {
		case RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED:
			snprintf(text, sizeof(text), "Achievement unlocked: %s (%u points)",
			         event->achievement->title, (unsigned)event->achievement->points);
			Say(text);
			break;
		case RC_CLIENT_EVENT_GAME_COMPLETED:
			Say(rc_client_get_hardcore_enabled(client) ? "Game mastered!" : "Game completed!");
			break;
		case RC_CLIENT_EVENT_LEADERBOARD_STARTED:
			snprintf(text, sizeof(text), "Leaderboard attempt: %s", event->leaderboard->title);
			Say(text);
			break;
		case RC_CLIENT_EVENT_LEADERBOARD_FAILED:
			snprintf(text, sizeof(text), "Leaderboard attempt failed: %s", event->leaderboard->title);
			Say(text);
			break;
		case RC_CLIENT_EVENT_LEADERBOARD_SUBMITTED:
			snprintf(text, sizeof(text), "Leaderboard: %s submitted (%s)",
			         event->leaderboard->title, event->leaderboard->tracker_value);
			Say(text);
			break;
		case RC_CLIENT_EVENT_SERVER_ERROR:
			snprintf(text, sizeof(text), "RetroAchievements server error: %s", event->server_error->error_message);
			Say(text);
			break;
		case RC_CLIENT_EVENT_DISCONNECTED:
			Say("RetroAchievements: connection lost, unlocks will be sent later");
			break;
		case RC_CLIENT_EVENT_RECONNECTED:
			Say("RetroAchievements: connection restored");
			break;
		case RC_CLIENT_EVENT_RESET:
			// Hardcore was switched back on : the game must restart from the
			// beginning. Same path as the in-game menu's « Reset game now! ».
			do_reset_game = true;
			break;
		default:
			break;
	}
}

static void RC_CCONV OnGameLoaded(int result, const char* error_message, rc_client_t* client, void*)
{
	if (result == RC_NO_GAME_LOADED) {
		Say("RetroAchievements: no achievements for this game");
		return;
	}
	if (result != RC_OK) {
		Say(std::string("RetroAchievements: ") + (error_message ? error_message : "game not loaded"));
		return;
	}
	// Les avertissements du serveur (« Unknown Emulator »...) passent pour des
	// succes d'identifiant 101000001 et plus : rc_client ne les compte pas, on
	// les affiche, c'est ce qui dit au joueur si ses succes peuvent compter.
	rc_client_achievement_list_t* all = rc_client_create_achievement_list(client,
		RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
	if (all) {
		for (uint32_t b = 0; b < all->num_buckets; b++)
			for (uint32_t i = 0; i < all->buckets[b].num_achievements; i++) {
				const rc_client_achievement_t* a = all->buckets[b].achievements[i];
				if (a->id >= 101000001u)
					Say(std::string("RetroAchievements: ") + a->title + " : " + a->description);
			}
		rc_client_destroy_achievement_list(all);
	}
	rc_client_user_game_summary_t summary;
	rc_client_get_user_game_summary(client, &summary);
	char text[160];
	if (summary.num_core_achievements == 0) {
		snprintf(text, sizeof(text), "RetroAchievements: no achievements for this game yet");
	} else {
		snprintf(text, sizeof(text), "RetroAchievements%s: %u of %u achievements unlocked",
		         rc_client_get_hardcore_enabled(client) ? " (hardcore)" : "",
		         summary.num_unlocked_achievements, summary.num_core_achievements);
	}
	Say(text);
}

static void RC_CCONV OnLogin(int result, const char* error_message, rc_client_t* client, void*)
{
	if (result != RC_OK) {
		Say(std::string("RetroAchievements: sign-in failed: ") + (error_message ? error_message : "unknown error"));
		return;
	}
	// The arcade hash is the set name : "mslug.zip" hashes as "mslug".
	std::string path = std::string(BurnDrvGetTextA(DRV_NAME)) + ".zip";
	rc_client_begin_identify_and_load_game(client, RC_CONSOLE_ARCADE, path.c_str(), NULL, 0, OnGameLoaded, NULL);
}

// The consoles FinalBurn Neo runs are not in the Arcade catalogue of
// RetroAchievements : only arcade boards are identified.
static bool IsArcade()
{
	switch ((UINT32)BurnDrvGetHardwareCode() & 0x7F000000) {
		case HARDWARE_PREFIX_SEGA_MEGADRIVE:
		case HARDWARE_PREFIX_PCENGINE:
		case HARDWARE_PREFIX_SEGA_MASTER_SYSTEM:
		case HARDWARE_PREFIX_SEGA_SG1000:
		case HARDWARE_PREFIX_COLECO:
		case HARDWARE_PREFIX_SEGA_GAME_GEAR:
		case HARDWARE_PREFIX_MSX:
		case HARDWARE_PREFIX_SPECTRUM:
		case HARDWARE_PREFIX_NES:
		case HARDWARE_PREFIX_FDS:
		case HARDWARE_PREFIX_NGP:
		case HARDWARE_PREFIX_CHANNELF:
		case HARDWARE_PREFIX_SNES:
		case HARDWARE_PREFIX_ASTROHOME:
		case HARDWARE_PREFIX_GBA:
			return false;
		default:
			return true;
	}
}

void CheevosInit()
{
	const char* user  = getenv("BOOTCADE_RA_USER");
	const char* token = getenv("BOOTCADE_RA_TOKEN");
	if (!user || !*user || !token || !*token) return;
	if (!IsArcade()) {
		printf("[Cheevos] %s is not an arcade game : achievements off\n", BurnDrvGetTextA(DRV_NAME));
		return;
	}
	const char* hc = getenv("BOOTCADE_RA_HARDCORE");
	bHardcore = !(hc && strcmp(hc, "0") == 0);
	std::string sUser = user, sToken = token;
	// Never handed down to anything this process starts.
	unsetenv("BOOTCADE_RA_TOKEN");

	MapMemory();
	if (sRegions.total_size == 0) {
		Say("RetroAchievements: this game's memory is not exposed, achievements off");
		rc_libretro_memory_destroy(&sRegions);
		return;
	}

	bStopping = false;
	pClient = rc_client_create(ReadMemory, ServerCall);
	char clause[64] = "";
	rc_client_get_user_agent_clause(pClient, clause, sizeof(clause));
	sUserAgent = std::string("FBNeo-Bootcade/") + szAppBurnVer + " (Linux) " + clause;
	rc_client_enable_logging(pClient, RC_CLIENT_LOG_LEVEL_WARN, LogMessage);
	rc_client_set_event_handler(pClient, OnEvent);
	rc_client_set_hardcore_enabled(pClient, bHardcore ? 1 : 0);
	rc_client_begin_login_with_token(pClient, sUser.c_str(), sToken.c_str(), OnLogin, NULL);
}

void CheevosExit()
{
	if (!pClient) return;
	bStopping = true;
	std::vector<std::thread> threads;
	{
		std::lock_guard<std::mutex> lock(sThreadsMutex);
		threads.swap(sThreads);
	}
	for (auto& t : threads) if (t.joinable()) t.join();
	rc_client_destroy(pClient);
	pClient = NULL;
	rc_libretro_memory_destroy(&sRegions);
	bHardcore = false;
}

void CheevosFrame()
{
	if (!pClient) return;
	rc_client_do_frame(pClient);
	FlushMessages();
}

void CheevosIdle()
{
	if (!pClient) return;
	rc_client_idle(pClient);
	FlushMessages();
}

void CheevosReset()
{
	if (pClient) rc_client_reset(pClient);
}

bool CheevosHardcore()
{
	return pClient && rc_client_get_hardcore_enabled(pClient);
}

bool CheevosRefuse(const char* what)
{
	if (!CheevosHardcore()) return false;
	char text[MESSAGE_MAX_LENGTH];
	snprintf(text, sizeof(text), "%s is disabled in RetroAchievements hardcore mode", what);
	UpdateMessage(text);
	return true;
}
