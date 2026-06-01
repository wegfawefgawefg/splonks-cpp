#pragma once

namespace splonks {

bool CheckGubsyShellSmoke();
bool CheckGubsyShellRealRoomdSmoke();
bool CheckGubsyShellRealnetLanHost(const char* server_url, int max_frames);
bool CheckGubsyShellRealnetLanClient(const char* server_url, const char* room_code,
                                     int max_frames);

} // namespace splonks
