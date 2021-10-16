#include "DojoGui.hpp"
#include <hw/naomi/naomi_cart.h>
namespace fs = ghc::filesystem;

void DojoGui::gui_display_bios_rom_warning(float scaling)
{
	std::string current_bios;
	if (settings.platform.system == DC_PLATFORM_NAOMI)
		current_bios = "naomi.zip";
	else if (settings.platform.system == DC_PLATFORM_ATOMISWAVE)
		current_bios = "awbios.zip";

	std::string file_path = settings.content.path;
	std::string current_filename = file_path.substr(file_path.find_last_of("/\\") + 1);

	std::string designation, start_msg, file_d;

	if (!bios_json_match && !current_json_match)
	{
		designation = "BIOS & ROM";
		start_msg = current_bios + " & " + current_filename + " do";
		file_d = "files";
	}
	else if (!bios_json_match)
	{
		designation = "BIOS";
		start_msg = current_bios + " does";
		file_d = "file";
	}
	else if (!current_json_match)
	{
		designation = "ROM";
		start_msg = current_filename + " does";
		file_d = "file";
	}

	std::string popup_title = designation + " Mismatch";

	ImGui::OpenPopup(popup_title.data());
	if (ImGui::BeginPopupModal(popup_title.data(), NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiInputTextFlags_EnterReturnsTrue))
	{
		std::string msg = start_msg + " not match the checksum of community-recommended " + file_d + ".\nPlease find a new " + designation + " and try again.";
		ImGui::TextColored(ImVec4(128, 0, 0, 1), "WARNING");
		ImGui::TextColored(ImVec4(128, 128, 0, 1), msg.data());
		std::string msg_2 = "Having a different " + designation + " than your opponent may result in desyncs.\nProceed at your own risk.";
		ImGui::Text(msg_2.data());

		if (ImGui::Button("Continue"))
		{
			if (config::TestGame)
			{
				if (strlen(settings.content.path.data()) > 0)
					gui_start_game(settings.content.path);
				else
					gui_state = GuiState::Main;
			}
			else if (config::GGPOEnable)
			{
				if (config::EnableMatchCode)
				{
					// use dojo session for match codes & initial handshake
					dojo.StartDojoSession();

					if (config::ActAsServer)
						gui_open_host_wait();
					else
						gui_open_guest_wait();
				}
				else
				{
					gui_open_ggpo_join();
				}
			}
			else if (config::DojoActAsServer)
			{
				dojo.host_status = 1;
				if (config::EnableLobby)
					dojo.remaining_spectators = config::Transmitting ? 1 : 0;
				else
					dojo.remaining_spectators = 0;
				gui_open_host_wait();
			}
			else
			{
				gui_open_guest_wait();
			}
		}

		/*
		ImGui::SameLine();
		if (ImGui::Button("Cancel"))
		{
			ImGui::CloseCurrentPopup();

			// Exit to main menu
			gui_state = GuiState::Main;
			game_started = false;
			settings.content.path = "";
			dc_reset(true);
		}
		*/

		ImGui::EndPopup();
	}

}

void DojoGui::gui_open_host_delay()
{
	gui_state = GuiState::HostDelay;
}

void DojoGui::gui_display_host_wait(float scaling)
{
	//emu.term();

	ImGui::SetNextWindowPos(ImVec2(settings.display.width / 2.f, settings.display.height / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(330 * scaling, 0));

	ImGui::Begin("##host_wait", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);

	ImGui::Text("Waiting for opponent to connect...");

	if (config::EnableMatchCode && !config::MatchCode.get().empty())
	{
		ImGui::Text("Match Code: %s", config::MatchCode.get().data());
		ImGui::SameLine();
		ShowHelpMarker("Match Codes not working?\nTry switching to Direct IP in the settings.");
		if (ImGui::Button("Copy Match Code"))
		{
			SDL_SetClipboardText(config::MatchCode.get().data());
		}
	}

	/*
	ImGui::SameLine();
	if (ImGui::Button("Cancel"))
	{
		ImGui::CloseCurrentPopup();

		// Exit to main menu
		gui_state = GuiState::Main;
		game_started = false;
		settings.content.path = "";
		dc_reset(true);
	}
	*/

	if (config::GGPOEnable)
	{
		if (!config::NetworkServer.get().empty())
		{
			gui_open_ggpo_join();
		}
	}
	else
	{
		if (!dojo.OpponentIP.empty())
		{
			dojo.host_status = 2;
			dojo.OpponentPing = dojo.DetectDelay(dojo.OpponentIP.data());

			gui_state = GuiState::Closed;
			gui_open_host_delay();
		}

		if (config::Transmitting &&
			dojo.remaining_spectators == 0)
		{
			if (config::EnableLobby)
				ImGui::Text("This match will be spectated.");
		}
	}

	ImGui::End();
}

void DojoGui::gui_display_guest_wait(float scaling)
{
	//emu.term();

	if (!config::GGPOEnable)
		dojo.pause();

	ImGui::SetNextWindowPos(ImVec2(settings.display.width / 2.f, settings.display.height / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(330 * scaling, 0));

	ImGui::Begin("##guest_wait", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);

	if (!dojo.client.name_acknowledged)
	{
		if ((!config::GGPOEnable && config::EnableMatchCode && config::MatchCode.get().empty()) ||
			(config::GGPOEnable && config::EnableMatchCode && config::NetworkServer.get().empty()))
		{
			ImGui::OpenPopup("Match Code");
			if (ImGui::BeginPopupModal("Match Code", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiInputTextFlags_EnterReturnsTrue))
			{
				ImGui::Text("Enter Match Code generated by host.");

				static char mc[128] = "";
				ImGui::InputTextWithHint("", "ABC123", mc, IM_ARRAYSIZE(mc), ImGuiInputTextFlags_CharsUppercase);

				ImGui::SameLine();
				if (ImGui::Button("Paste"))
				{
					char* pasted_txt = SDL_GetClipboardText();
					memcpy(mc, pasted_txt, strlen(pasted_txt));
				}

				if (ImGui::Button("Start Session"))
				{
					dojo.MatchCode = std::string(mc, strlen(mc));
					ImGui::CloseCurrentPopup();
				}

				/*
				ImGui::SameLine();
				if (ImGui::Button("Cancel"))
				{
					ImGui::CloseCurrentPopup();

					// Exit to main menu
					gui_state = GuiState::Main;
					game_started = false;
					settings.content.path = "";
					dc_reset(true);
				}
				*/

				ImGui::EndPopup();
			}
		}
		else if (!config::GGPOEnable && config::DojoServerIP.get().empty())
		{
   			ImGui::OpenPopup("Connect to Host Server");
   			if (ImGui::BeginPopupModal("Connect to Host Server", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiInputTextFlags_EnterReturnsTrue))
   			{
   				ImGui::Text("Enter Host Server Details");

   				static char si[128] = "";
   				ImGui::InputTextWithHint("IP", "0.0.0.0", si, IM_ARRAYSIZE(si));

					ImGui::SameLine();
					if (ImGui::Button("Paste"))
					{
						char* pasted_txt = SDL_GetClipboardText();
						memcpy(si, pasted_txt, strlen(pasted_txt));
					}

   				static char sp[128] = "6000";
   				ImGui::InputTextWithHint("Port", "6000", sp, IM_ARRAYSIZE(sp));

   				if (ImGui::Button("Start Session"))
   				{
					config::DojoServerIP = std::string(si, strlen(si));
					config::DojoServerPort = std::string(sp, strlen(sp));
					cfgSaveStr("dojo", "ServerIP", config::DojoServerIP.get().c_str());
					cfgSaveStr("dojo", "ServerPort", config::DojoServerPort.get().c_str());

					dojo.client.SetHost(config::DojoServerIP, atoi(config::DojoServerPort.get().data()));

   					ImGui::CloseCurrentPopup();
   				}
				
				ImGui::SameLine();
				if (ImGui::Button("Cancel"))
				{
					ImGui::CloseCurrentPopup();

					// Exit to main menu
					gui_state = GuiState::Main;
					game_started = false;
					settings.content.path = "";
					dc_reset(true);

					config::DojoServerIP = "";
					cfgSaveStr("dojo", "ServerIP", config::DojoServerIP.get().c_str());
				}

   				ImGui::EndPopup();
   			}
		}

		if (!config::GGPOEnable && !config::DojoServerIP.get().empty())
		{
			ImGui::Text("Connecting to host...");

			/*
			ImGui::SameLine();
			if (ImGui::Button("Cancel"))
			{
				ImGui::CloseCurrentPopup();

				// Exit to main menu
				gui_state = GuiState::Main;
				game_started = false;
				settings.content.path = "";
				dc_reset(true);

				config::DojoServerIP = "";
				cfgSaveStr("dojo", "ServerIP", config::DojoServerIP.data().c_str());
			}
			*/

			dojo.client.SendPlayerName();
		}
	}
	else
	{
		ImGui::Text("Waiting for host to select delay...");
	}

	if (config::GGPOEnable && !config::NetworkServer.get().empty())
		gui_open_ggpo_join();

	if (dojo.session_started)
	{
		gui_state = GuiState::Closed;
		dojo.resume();
	}

	ImGui::End();
}

void DojoGui::gui_display_ggpo_join(float scaling)
{
	std::string title = config::EnableMatchCode ? "Select GGPO Frame Delay" : "Connect to GGPO Opponent";
	ImGui::OpenPopup(title.data());
	if (ImGui::BeginPopupModal(title.data(), NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiInputTextFlags_EnterReturnsTrue))
	{
		static char si[128] = "";
		std::string detect_address = "";

		if (config::EnableMatchCode)
		{
			detect_address = config::NetworkServer.get();

			// if both player names are defaults, hide names
			if (!config::GGPOEnable)
			{
				if (!(config::PlayerName.get().compare("Player") == 0 &&
					config::PlayerName.get().compare(config::OpponentName.get()) == 0))
				{
					if (config::ActAsServer)
						ImGui::Text("%s vs %s", config::PlayerName.get().c_str(), config::OpponentName.get().c_str());
					else
						ImGui::Text("%s vs %s", config::OpponentName.get().c_str(), config::PlayerName.get().c_str());
				}
			}
		}
		else
		{
			if (config::NetworkServer.get().empty())
			{
				if (config::ShowPublicIP)
				{
					static char external_ip[128] = "";
					if (current_public_ip.empty())
					{
						current_public_ip = dojo.GetExternalIP();
						const char* external = current_public_ip.data();
						memcpy(external_ip, external, strlen(external));
					}

					ImGui::PushStyleColor(ImGuiCol_TextDisabled, ImVec4(255, 255, 0, 1));
					ImGui::Text("Your Public IP: ");
					ImGui::SameLine();
					ImGui::TextDisabled("%s", external_ip);
					ImGui::PopStyleColor();
					ImGui::SameLine();
					ShowHelpMarker("This is your public IP to share with your opponent.\nFor Virtual LANs, refer to your software.");

					ImGui::SameLine();
					if (ImGui::Button("Copy"))
					{
						SDL_SetClipboardText(current_public_ip.data());
					}
				}

				ImGui::InputTextWithHint("IP", "0.0.0.0", si, IM_ARRAYSIZE(si));
				detect_address = std::string(si);
				ImGui::SameLine();
				if (ImGui::Button("Paste"))
				{
					char* pasted_txt = SDL_GetClipboardText();
					memcpy(si, pasted_txt, strlen(pasted_txt));
				}
			}
		}

		ImGui::SliderInt("", (int*)&dojo.current_delay, 0, 20);
		ImGui::SameLine();
		ImGui::Text("Delay");

		if (config::EnableMatchCode)
		{
			if (ImGui::Button("Detect Delay"))
				dojo.OpponentPing = dojo.DetectGGPODelay(detect_address.data());

			if (dojo.OpponentPing > 0)
			{
				ImGui::SameLine();
				ImGui::Text("Current Ping: %d ms", dojo.OpponentPing);
			}
		}

		if (ImGui::Button("Start Session"))
		{
			if (dojo.current_delay != config::GGPODelay.get())
				config::GGPODelay.set(dojo.current_delay);

			if (!config::EnableMatchCode)
			{
				config::NetworkServer.set(std::string(si, strlen(si)));
				cfgSaveStr("network", "server", config::NetworkServer.get());

				config::DojoEnable = false;
			}
			ImGui::CloseCurrentPopup();
			start_ggpo();
		}

		ImGui::SameLine();
		if (ImGui::Button("Cancel"))
		{
		    ImGui::CloseCurrentPopup();

		    // Exit to main menu
		    gui_state = GuiState::Main;
		    game_started = false;
		    settings.content.path = "";
		    dc_reset(true);

		    config::NetworkServer.set("");
		}

		ImGui::EndPopup();
	}
}

void DojoGui::gui_display_disconnected( float scaling)
{
	ImGui::SetNextWindowPos(ImVec2(settings.display.width / 2.f, settings.display.height / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(330 * scaling, 0));

	ImGui::Begin("##disconnected", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);

	if (dojo.client.opponent_disconnected)
		ImGui::Text("Opponent disconnected.");
	else
		ImGui::Text("Disconnected.");

	ImGui::End();

	config::AutoSkipFrame = 1;

	dojo.CleanUp();

	error_popup();
}

void DojoGui::gui_display_end_replay( float scaling)
{
	ImGui::SetNextWindowPos(ImVec2(settings.display.width / 2.f, settings.display.height / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(330 * scaling, 0));

	ImGui::Begin("##end_replay", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);

	ImGui::Text("End of replay.");

	ImGui::End();
}

void DojoGui::gui_display_end_spectate( float scaling)

{
	emu.term();

	ImGui::SetNextWindowPos(ImVec2(settings.display.width / 2.f, settings.display.height / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(330 * scaling, 0));

	ImGui::Begin("##end_spectate", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);

	ImGui::Text("End of spectated match.");

	ImGui::End();
}

void DojoGui::gui_display_host_delay( float scaling)

{
	//emu.term();

	dojo.pause();

	ImGui::SetNextWindowPos(ImVec2(settings.display.width / 2.f, settings.display.height / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(360 * scaling, 0));

	ImGui::Begin("##host_delay", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);

	// if both player names are defaults, hide names
	if (!(config::PlayerName.get().compare("Player") == 0 &&
		config::PlayerName.get().compare(config::OpponentName.get()) == 0))
	{
		ImGui::Text("%s vs %s", config::PlayerName.get().c_str(), config::OpponentName.get().c_str());
	}

	ImGui::SliderInt("", (int*)&config::Delay.get(), 1, 10);
	ImGui::SameLine();
	ImGui::Text("Set Delay");

	if (ImGui::Button("Detect Delay"))
		dojo.OpponentPing = dojo.DetectDelay(dojo.OpponentIP.data());

	if (dojo.OpponentPing > 0)
	{
		ImGui::SameLine();
		ImGui::Text("Current Ping: %d ms", dojo.OpponentPing);
	}

	if (ImGui::Button("Start Game"))
	{
		dojo.PlayMatch = false;
		if (config::Transmitting &&
			dojo.remaining_spectators == 0)
		{
			dojo.StartTransmitterThread();
		}

		dojo.isMatchStarted = true;
		dojo.StartSession(config::Delay.get(),
			config::PacketsPerFrame,
			config::NumBackFrames);
		dojo.resume();

		gui_state = GuiState::Closed;
	}

	SaveSettings();

	ImGui::End();
}

void DojoGui::gui_display_test_game( float scaling)

{
	emu.term();

	ImGui::SetNextWindowPos(ImVec2(settings.display.width / 2.f, settings.display.height / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(330 * scaling, 0));

	ImGui::Begin("##test_game", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);

	ImGui::Columns(2, "buttons", false);
	if (ImGui::Button("Settings", ImVec2(150 * scaling, 50 * scaling)))
	{
		gui_state = GuiState::Settings;
	}
	ImGui::NextColumn();
	if (ImGui::Button("Start Game", ImVec2(150 * scaling, 50 * scaling)))
	{
		gui_state = GuiState::Closed;

		if (!settings.dojo.GameEntry.empty())
		{
			try {
				std::string entry_path = dojo_file.GetEntryPath(settings.dojo.GameEntry);
				settings.content.path = entry_path;
			}
			catch (...) { }
		}
		if (strlen(settings.content.path.data()) > 0)
		{
			std::string extension = get_file_extension(settings.content.path);
			// dreamcast games use built-in bios by default
			if (extension == "chd" || extension == "gdi" || extension == "cdi")
			{
				dojo_gui.bios_json_match = true;
				settings.platform.system = DC_PLATFORM_DREAMCAST;
			}
			else
			{
				int platform = naomi_cart_GetPlatform(settings.content.path.data());
				settings.platform.system = platform;
				//dojo_gui.bios_json_match = dojo_file.CompareBIOS(platform);
			}

			/*
			dojo_gui.current_json_match = dojo_file.CompareRom(settings.content.path);

			if (!dojo_gui.bios_json_match || !dojo_gui.current_json_match)
				gui_state = GuiState::BiosRomWarning;
			else
			*/

			gui_start_game(settings.content.path);
		}
		else
		{
			gui_state = GuiState::Main;
		}

	}
#ifdef _WIN32
	if (!settings.dojo.GameEntry.empty())
	{
		try {
			std::string entry_path = dojo_file.GetEntryPath(settings.dojo.GameEntry);
			settings.content.path = entry_path;
		}
		catch (...) { }
	}

	std::map<std::string, std::string> game_links = dojo_file.GetFileResourceLinks(settings.content.path);
	if (!game_links.empty())
	{
		for (std::pair<std::string, std::string> link: game_links)
		{
			ImGui::NextColumn();
			if (ImGui::Button(std::string("Open " + link.first).data(), ImVec2(150 * scaling, 50 * scaling)))
				ShellExecute(0, 0, link.second.data(), 0, 0, SW_SHOW);
		}
	}
#endif

	ImGui::End();
}

std::vector<std::string> split(const std::string& text, char delimiter) {
    std::string tmp;
    std::vector<std::string> stk;
    std::stringstream ss(text);
    while(getline(ss,tmp, delimiter)) {
        stk.push_back(tmp);
    }
    return stk;
}

void DojoGui::gui_display_lobby(float scaling, std::vector<GameMedia> game_list)
{
	if (!dojo.lobby_active)
	{
		std::thread t4(&DojoLobby::ListenerThread, std::ref(dojo.presence));
		t4.detach();
	}

	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::SetNextWindowSize(ImVec2(settings.display.width, settings.display.height));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);

	ImGui::Begin("Lobby", NULL, /*ImGuiWindowFlags_AlwaysAutoResize |*/ ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
	ImVec2 normal_padding = ImGui::GetStyle().FramePadding;
	
	if (ImGui::Button("Done", ImVec2(100 * scaling, 30 * scaling)))
	{
		if (game_started)
    		gui_state = GuiState::Commands;
    	else
    		gui_state = GuiState::Main;
	}

	ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("Host Game").x - ImGui::GetStyle().FramePadding.x * 2.0f - ImGui::GetStyle().ItemSpacing.x);
	if (ImGui::Button("Host Game", ImVec2(100 * scaling, 30 * scaling)))
	{
		config::ActAsServer = true;
		cfgSaveBool("dojo", "ActAsServer", config::ActAsServer);
		gui_state = GuiState::Main;
	}

	ImGui::Columns(4, "mycolumns"); // 4-ways, with border
	ImGui::Separator();
	ImGui::SetColumnWidth(0, ImGui::CalcTextSize("Ping").x + ImGui::GetStyle().FramePadding.x * 2.0f + ImGui::GetStyle().ItemSpacing.x);
	//ImGui::Text("Ping"); ImGui::NextColumn();
	ImGui::Text("    "); ImGui::NextColumn();
	ImGui::Text("Players"); ImGui::NextColumn();
	ImGui::Text("Status"); ImGui::NextColumn();
	ImGui::Text("Game"); ImGui::NextColumn();
	ImGui::Separator();

	std::map<std::string, std::string> beacons = dojo.presence.active_beacons;
	for (auto it = beacons.cbegin(); it != beacons.cend(); ++it) {
		std::string s = it->second;
		std::string delimiter = "__";

		std::string beacon_id = it->first;
		std::vector<std::string> beacon_entry;

		if (dojo.presence.last_seen[beacon_id] + 10000 > dojo.unix_timestamp())
		{
			size_t pos = 0;
			std::string token;
			while ((pos = s.find(delimiter)) != std::string::npos) {
			    token = s.substr(0, pos);
			    std::cout << token << std::endl;
				beacon_entry.push_back(token);
			    s.erase(0, pos + delimiter.length());
			}

			std::string beacon_ip = beacon_entry[0];
			std::string beacon_server_port = beacon_entry[1];
			std::string beacon_status = beacon_entry[2];

			std::string beacon_game = beacon_entry[3].c_str();
			std::string beacon_game_path = "";

			std::string beacon_remaining_spectators = beacon_entry[5];

			std::vector<GameMedia> games = game_list;
			std::vector<GameMedia>::iterator it = std::find_if (games.begin(), games.end(),
				[&](GameMedia gm) { return ( gm.name.rfind(beacon_game, 0) == 0 ); });

			if (it != games.end())
			{
				beacon_game = it->name;
				beacon_game_path = it->path;
			}

			std::string beacon_player = beacon_entry[4];

			bool is_selected;
			int beacon_ping = dojo.presence.active_beacon_ping[beacon_id];
			std::string beacon_ping_str = "";
			if (beacon_ping > 0)
				beacon_ping_str = std::to_string(beacon_ping);

			if (beacon_status == "Hosting, Waiting" &&
				ImGui::Selectable(beacon_ping_str.c_str(), &is_selected, ImGuiSelectableFlags_SpanAllColumns))
			{
				dojo.PlayMatch = false;

				config::ActAsServer = false;
				config::DojoServerIP = beacon_ip;
				config::DojoServerPort = beacon_server_port;

				SaveSettings();

				gui_state = GuiState::Closed;
				gui_start_game(beacon_game_path);
			}

			std::string popup_name = "Options " + beacon_id;
			if (ImGui::BeginPopupContextItem(popup_name.c_str(), 1))
			{
				if (beacon_remaining_spectators == "0")
				{
					ImGui::MenuItem("Spectate", NULL, false, false);
				}
				else
				{
					if (ImGui::MenuItem("Spectate"))
					{
						dojo.PlayMatch = false;

						config::Receiving = true;
						dojo.receiving = true;

						config::ActAsServer = false;
						dojo.hosting = false;

						dojo.RequestSpectate(beacon_ip, beacon_server_port);

						gui_start_game(beacon_game_path);
					}
				}

				ImGui::EndPopup();
			}

			ImGui::NextColumn();

			ImGui::Text(beacon_player.c_str());  ImGui::NextColumn();
			ImGui::Text(beacon_status.c_str());  ImGui::NextColumn();
			ImGui::Text(beacon_game.c_str()); ImGui::NextColumn();
		}
	}

    ImGui::End();
    ImGui::PopStyleVar();
}

void DojoGui::show_playback_menu(float scaling, bool paused)
{
	if (config::EnablePlayerNameOverlay)
		show_player_name_overlay(scaling, true);

	if (!config::ShowPlaybackControls)
	{
		return;
	}

	unsigned int total;
	if (dojo.replay_version == 1)
		total = dojo.net_inputs[0].size();
	else
		total = dojo.maple_inputs.size() - 1;

	int position = dojo.FrameNumber.load();

	if (!paused)
	{
		//ImGui_Impl_NewFrame();
		ImGui::NewFrame();
	}

	ImGui::SetNextWindowBgAlpha(0.6f);
	ImGui::SetNextWindowPos(ImVec2((settings.display.width / 2) - 150, settings.display.height - 45));
	ImGui::SetNextWindowSize(ImVec2(350, 40));

	ImGui::Begin("##fn", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration );

	ImGui::SliderInt("", &position, 0, total);
	ImGui::SameLine();
	ImGui::Text("%u", total);

	ImGui::SameLine();

	/*
	if (dojo.replay_version == 1)
	{
		if (!paused)
		{
			if (ImGui::Button("Pause"))
			{
				TermAudio();
				gui_state = GuiState::ReplayPause;
			}

			ImGui::SameLine();
			if (ImGui::Button("Hide"))
			{
				config::ShowPlaybackControls = false;
			}
		}
		else
		{
			if (ImGui::Button("Play"))
			{
				InitAudio();
				gui_state = GuiState::Closed;
			}
		}
	}
	*/

	ImGui::End();

	if (!paused)
	{
		ImGui::Render();
		ImGui_impl_RenderDrawData(ImGui::GetDrawData());
	}
}

void DojoGui::show_player_name_overlay(float scaling, bool paused)
{
	// if both player names are defaults, hide overlay
	if (dojo.player_2.length() <= 1 ||
		strcmp(dojo.player_1.data(), "Player") == 0 &&
		strcmp(dojo.player_1.data(), dojo.player_2.data()) == 0)
	{
		return;
	}

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
	ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.557f, 0.268f, 0.965f, 1.f));

	if (dojo.player_1.length() > 1)
	{
		float font_size = ImGui::CalcTextSize(dojo.player_1.data()).x;

		ImGui::SetNextWindowPos(ImVec2((settings.display.width / 4) - ((font_size + 20) / 2), 0));
		ImGui::SetNextWindowSize(ImVec2(font_size + 20, 35));
		ImGui::SetNextWindowBgAlpha(0.5f);
		ImGui::Begin("#one", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs);

		ImGui::SameLine(
			(ImGui::GetContentRegionAvail().x / 2) -
			font_size + (font_size / 2) + 10 
		);

		ImGui::Text(dojo.player_1.c_str());

		ImGui::End();
	}

	if (dojo.player_2.length() > 1)
	{
		float font_size = ImGui::CalcTextSize(dojo.player_2.data()).x;

		ImGui::SetNextWindowPos(ImVec2(((settings.display.width / 4) * 3) - ((font_size + 20) / 2), 0));
		ImGui::SetNextWindowSize(ImVec2(font_size + 20, 35));
		ImGui::SetNextWindowBgAlpha(0.5f);
		ImGui::Begin("#two", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs);

		ImGui::SameLine(
			(ImGui::GetContentRegionAvail().x / 2) -
			font_size + (font_size / 2) + 10 
		);

		ImGui::Text(dojo.player_2.c_str());
		ImGui::End();
	}

	ImGui::PopStyleColor();
	ImGui::PopStyleVar(2);
}

void DojoGui::gui_display_paused(float scaling)
{
	show_playback_menu(scaling, true);
}

void DojoGui::gui_display_replays(float scaling, std::vector<GameMedia> game_list)
{
	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::SetNextWindowSize(ImVec2(settings.display.width, settings.display.height));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);

	ImGui::Begin("Replays", NULL, /*ImGuiWindowFlags_AlwaysAutoResize |*/ ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
	ImVec2 normal_padding = ImGui::GetStyle().FramePadding;

	if (ImGui::Button("Done", ImVec2(100 * scaling, 30 * scaling)))
	{
		cfgSaveBool("dojo", "RecordMatches", config::RecordMatches);
		if (game_started)
			gui_state = GuiState::Commands;
		else
			gui_state = GuiState::Main;
	}

	ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("Record All Sessions").x - ImGui::GetStyle().FramePadding.x * 4.0f - ImGui::GetStyle().ItemSpacing.x * 4);

	OptionCheckbox("Record All Sessions", config::RecordMatches);
	ImGui::SameLine();
	ShowHelpMarker("Record all netplay sessions to a local file");

	ImGui::Columns(3, "mycolumns"); // 4-ways, with border
	ImGui::Separator();
	ImGui::Text("Date"); ImGui::NextColumn();
	ImGui::Text("Players"); ImGui::NextColumn();
	ImGui::Text("Game"); ImGui::NextColumn();
	ImGui::Separator();

	fs::path path = fs::current_path() / "replays";

	if (!ghc::filesystem::exists(path))
		ghc::filesystem::create_directory(path);

	std::map<std::string, std::string> replays;
	for (auto& p : fs::directory_iterator(path))
	{
		std::string replay_path = p.path().string();

		if (stringfix::get_extension(replay_path) == "flyreplay" ||
			stringfix::get_extension(replay_path) == "flyr")
		{

			std::string s = replay_path;
			std::string delimiter = "__";

			std::vector<std::string> replay_entry;

			size_t pos = 0;
			std::string token;
			while ((pos = s.find(delimiter)) != std::string::npos) {
				token = s.substr(0, pos);
				//std::cout << token << std::endl;
				replay_entry.push_back(token);
				s.erase(0, pos + delimiter.length());
			}

#ifdef _WIN32
			std::string game_name = replay_entry[0].substr(replay_entry[0].rfind("\\") + 1);
#else
			std::string game_name = replay_entry[0].substr(replay_entry[0].rfind("/") + 1);
#endif

			std::string date = replay_entry[1];
			std::string host_player = replay_entry[2];
			std::string guest_player = replay_entry[3];

			std::string game_path = "";

			std::vector<GameMedia> games = game_list;
			std::vector<GameMedia>::iterator it = std::find_if(games.begin(), games.end(),
				[&](GameMedia gm) { return (gm.name.rfind(game_name, 0) == 0); });

			if (it != games.end())
			{
				game_path = it->path;
			}

			bool is_selected = false;
			if (ImGui::Selectable(date.c_str(), &is_selected, ImGuiSelectableFlags_SpanAllColumns))
			{
				dojo.ReplayFilename = replay_path;
				dojo.PlayMatch = true;

				gui_state = GuiState::Closed;
				//dojo.StartDojoSession();

				if (guest_player.empty())
					dojo.offline_replay = true;

				config::DojoEnable = true;
				gui_start_game(game_path);
			}
			ImGui::NextColumn();

			std::string players = host_player;
			if (!guest_player.empty())
				players += " vs " + guest_player;

			ImGui::Text(players.c_str());  ImGui::NextColumn();
			ImGui::Text(game_name.c_str());  ImGui::NextColumn();
		}
	}

    ImGui::End();
    ImGui::PopStyleVar();
}

void DojoGui::insert_netplay_tab(ImVec2 normal_padding)
{
	if (ImGui::BeginTabItem("Dojo"))
	{
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, normal_padding);

		OptionCheckbox("Enable Player Name Overlay", config::EnablePlayerNameOverlay);
		ImGui::SameLine();
		ShowHelpMarker("Enable overlay showing player names during netplay sessions & replays");

		char PlayerName[256] = { 0 };
		strcpy(PlayerName, config::PlayerName.get().c_str());
		ImGui::InputText("Player Name", PlayerName, sizeof(PlayerName), ImGuiInputTextFlags_CharsNoBlank, nullptr, nullptr);
		ImGui::SameLine();
		ShowHelpMarker("Name visible to other players");
		config::PlayerName = std::string(PlayerName, strlen(PlayerName));

		if (ImGui::CollapsingHeader("Netplay Method", ImGuiTreeNodeFlags_DefaultOpen))
		{
			int netplayMethod;
			if (config::NetplayMethod.get() == "Delay")
				netplayMethod = 0;
			else
				netplayMethod = 1;

			ImGui::Columns(2, "netplayMethod", false);
			ImGui::RadioButton("Delay", &netplayMethod, 0);
			ImGui::NextColumn();
			ImGui::RadioButton("GGPO", &netplayMethod, 1);
			ImGui::NextColumn();
			ImGui::Columns(1, NULL, false);

			switch (netplayMethod)
			{
			case 0:
				config::NetplayMethod = "Delay";
				break;
			case 1:
				config::NetplayMethod = "GGPO";
				break;
			}

		}

		if (ImGui::CollapsingHeader("Connection Method", ImGuiTreeNodeFlags_DefaultOpen))
		{
			int cxnMethod;
			if (!config::EnableMatchCode)
				cxnMethod = 0;
			else
				cxnMethod = 1;

			ImGui::Columns(2, "cxnMethod", false);
			ImGui::RadioButton("Direct IP", &cxnMethod, 0);
			ImGui::SameLine();
			ShowHelpMarker("Connect to IP Address.\nCan either be Public IP or Private IP via LAN.");
			ImGui::NextColumn();
			ImGui::RadioButton("Match Codes", &cxnMethod, 1);
			ImGui::SameLine();
			ShowHelpMarker("Establishes direct connection via public matchmaking relay.\nWorks with most home routers.");
			ImGui::NextColumn();
			ImGui::Columns(1, NULL, false);

			switch (cxnMethod)
			{
			case 0:
				config::EnableMatchCode = false;
				break;
			case 1:
				config::EnableMatchCode = true;
				break;
			}
		}

		if (config::NetplayMethod.get() == "GGPO")
		{
			if (!config::EnableMatchCode)
			{
				OptionCheckbox("Show Public IP on Connection Dialog", config::ShowPublicIP);
			}

			if (ImGui::CollapsingHeader("GGPO", ImGuiTreeNodeFlags_DefaultOpen))
			{
				OptionCheckbox("Show Network Statistics Overlay", config::NetworkStats,
					"Display network statistics on screen");

				ImGui::Text("Left Thumbstick:");
				OptionRadioButton<int>("Disabled", config::GGPOAnalogAxes, 0, "Left thumbstick not used");
				ImGui::SameLine();
				OptionRadioButton<int>("Horizontal", config::GGPOAnalogAxes, 1, "Use the left thumbstick horizontal axis only");
				ImGui::SameLine();
				OptionRadioButton<int>("Full", config::GGPOAnalogAxes, 2, "Use the left thumbstick horizontal and vertical axes");
			}
		}

		if (ImGui::CollapsingHeader("Replays", ImGuiTreeNodeFlags_DefaultOpen))
		{
			OptionCheckbox("Show Frame Position", config::ShowPlaybackControls);
			ImGui::SameLine();
			ShowHelpMarker("Shows current frame position on playback.");

			OptionCheckbox("Record All Sessions", config::RecordMatches);
			ImGui::SameLine();
			ShowHelpMarker("Record all gameplay sessions to a local file");
		}

		if (ImGui::CollapsingHeader("Netplay", ImGuiTreeNodeFlags_DefaultOpen))
		{
			OptionCheckbox("Enable UPnP", config::EnableUPnP);
			ImGui::SameLine();
			ShowHelpMarker("Enable Universal Plug & Play for game sessions.");

			if (config::NetplayMethod.get() == "GGPO")
			{
				int GGPOPort = config::GGPOPort.get();
				ImGui::InputInt("GGPO Local Port", &GGPOPort);
				ImGui::SameLine();
				ShowHelpMarker("The GGPO port to listen on");
				if (GGPOPort != config::GGPOPort.get())
					config::GGPOPort = GGPOPort;

				int GGPORemotePort = config::GGPORemotePort.get();
				ImGui::InputInt("GGPO Remote Port", &GGPORemotePort);
				ImGui::SameLine();
				ShowHelpMarker("The GGPO destination port to transmit to");
				if (GGPORemotePort != config::GGPORemotePort.get())
					config::GGPORemotePort = GGPORemotePort;

			}

			std::string PortTitle;
			std::string PortDescription;

			if (config::NetplayMethod.get() == "Delay")
			{
				PortTitle = "Server Port";
				PortDescription = "The server port to listen on";
			}
			else
			{
				PortTitle = "Handshake Port";
				PortDescription = "The handshake port to listen on";
			}

			char ServerPort[256];
			strcpy(ServerPort, config::DojoServerPort.get().c_str());

			ImGui::InputText(PortTitle.c_str(), ServerPort, sizeof(ServerPort), ImGuiInputTextFlags_CharsNoBlank, nullptr, nullptr);
			ImGui::SameLine();
			ShowHelpMarker(PortDescription.c_str());
			config::DojoServerPort = ServerPort;

			if (!config::EnableLobby)
			{
				if (config::EnableMatchCode)
				{
					if (ImGui::CollapsingHeader("Match Codes", ImGuiTreeNodeFlags_DefaultOpen))
					{
						char MatchmakingServerAddress[256];

						strcpy(MatchmakingServerAddress, config::MatchmakingServerAddress.get().c_str());
						ImGui::InputText("Matchmaking Service Address", MatchmakingServerAddress, sizeof(MatchmakingServerAddress), ImGuiInputTextFlags_CharsNoBlank, nullptr, nullptr);
						config::MatchmakingServerAddress = MatchmakingServerAddress;

						char MatchmakingServerPort[256];

						strcpy(MatchmakingServerPort, config::MatchmakingServerPort.get().c_str());
						ImGui::InputText("Matchmaking Service Port", MatchmakingServerPort, sizeof(MatchmakingServerPort), ImGuiInputTextFlags_CharsNoBlank, nullptr, nullptr);
						config::MatchmakingServerPort = MatchmakingServerPort;
					}
				}
			}

			if (!config::EnableMatchCode && config::NetplayMethod.get() == "Delay")
			{
				OptionCheckbox("Enable LAN Lobby", config::EnableLobby);
				ImGui::SameLine();
				ShowHelpMarker("Enable discovery and matchmaking over LAN");

				if (config::EnableLobby)
				{
					if (ImGui::CollapsingHeader("LAN Lobby", ImGuiTreeNodeFlags_DefaultOpen))
					{
						char LobbyMulticastAddress[256];

						strcpy(LobbyMulticastAddress, config::LobbyMulticastAddress.get().c_str());
						ImGui::InputText("Lobby Multicast Address", LobbyMulticastAddress, sizeof(LobbyMulticastAddress), ImGuiInputTextFlags_CharsNoBlank, nullptr, nullptr);
						ImGui::SameLine();
						ShowHelpMarker("Multicast IP Address for Lobby to Target");
						config::LobbyMulticastAddress = LobbyMulticastAddress;

						char LobbyMulticastPort[256];

						strcpy(LobbyMulticastPort, config::LobbyMulticastPort.get().c_str());
						ImGui::InputText("Lobby Multicast Port", LobbyMulticastPort, sizeof(LobbyMulticastPort), ImGuiInputTextFlags_CharsNoBlank, nullptr, nullptr);
						ImGui::SameLine();
						ShowHelpMarker("Multicast Port for Lobby to Target");
						config::LobbyMulticastPort = LobbyMulticastPort;
					}
				}
			}
		}

		if (ImGui::CollapsingHeader("Session Streaming", ImGuiTreeNodeFlags_DefaultOpen))
		{
			OptionCheckbox("Enable Session Transmission", config::Transmitting);
			ImGui::SameLine();
			ShowHelpMarker("Transmit netplay sessions as TCP stream to target spectator");

			if (config::Transmitting)
			{
				char SpectatorIP[256];

				strcpy(SpectatorIP, config::SpectatorIP.get().c_str());
				ImGui::InputText("Spectator IP Address", SpectatorIP, sizeof(SpectatorIP), ImGuiInputTextFlags_CharsNoBlank, nullptr, nullptr);
				ImGui::SameLine();
				ShowHelpMarker("Target Spectator IP Address");
				config::SpectatorIP = SpectatorIP;

				/*
				OptionCheckbox("Transmit Replays", config::TransmitReplays);
				ImGui::SameLine();
				ShowHelpMarker("Transmit replays to target spectator");
				*/
			}

			char SpectatorPort[256];

			strcpy(SpectatorPort, config::SpectatorPort.get().c_str());
			ImGui::InputText("Spectator Port", SpectatorPort, sizeof(SpectatorPort), ImGuiInputTextFlags_CharsNoBlank, nullptr, nullptr);
			ImGui::SameLine();
			ShowHelpMarker("Port to send or receive session streams");
			config::SpectatorPort = SpectatorPort;

			int one = 1;
			ImGui::InputScalar("Frame Buffer", ImGuiDataType_S32, &config::RxFrameBuffer.get(), &one, NULL, "%d");
			ImGui::SameLine();
			ShowHelpMarker("# of frames to cache before playing received match stream");
		}

		if (ImGui::CollapsingHeader("Memory Management", ImGuiTreeNodeFlags_None))
		{
			OptionCheckbox("Validate BIOS & ROMs before netplay session", config::NetStartVerifyRoms);
			ImGui::SameLine();
			ShowHelpMarker("Validates BIOSes & ROMs against provided flycast_roms.json. Ensures proper files for retrieved savestates and known netplay games.");

			if (config::NetplayMethod.get() == "Delay")
			{
				OptionCheckbox("Enable NVMEM/EEPROM Restoration", config::EnableMemRestore);
				ImGui::SameLine();
				ShowHelpMarker("Restores NVMEM & EEPROM files before netplay session to prevent desyncs. Disable if you wish to use modified files with your opponent. (i.e., palmods, custom dipswitches)");

				OptionCheckbox("Ignore Existing Netplay Savestates", config::IgnoreNetSave);
				ImGui::SameLine();
				ShowHelpMarker("Ignore previously generated or custom savestates ending in .net.");
			}

			OptionCheckbox("Allow Custom VMUs", config::NetCustomVmu);
			ImGui::SameLine();
			ShowHelpMarker("Allows custom VMUs for netplay ending in .bin.net. VMU must match opponent's. Deletes and regenerates blank Dreamcast VMUs for netplay when disabled.");

			char NetSaveBase[256];

			strcpy(NetSaveBase, config::NetSaveBase.get().c_str());
			ImGui::InputText("Savestate Repository URL", NetSaveBase, sizeof(NetSaveBase), ImGuiInputTextFlags_CharsNoBlank, nullptr, nullptr);
			config::NetSaveBase = NetSaveBase;
		}

		if (config::NetplayMethod.get() == "Delay")
		{
			if (ImGui::CollapsingHeader("Advanced", ImGuiTreeNodeFlags_None))
			{
				OptionCheckbox("Enable Session Quick Load", config::EnableSessionQuickLoad);
				ImGui::SameLine();
				ShowHelpMarker("Saves state at common frame after boot. Allows you to press 'Quick Load' to revert to common state for both players. (Manually set on both sides)");

				ImGui::SliderInt("Packets Per Frame", (int*)&config::PacketsPerFrame.get(), 1, 10);
				ImGui::SameLine();
				ShowHelpMarker("Number of packets to send per input frame.");

				OptionCheckbox("Enable Backfill", config::EnableBackfill);
				ImGui::SameLine();
				ShowHelpMarker("Transmit past input frames along with current one in packet payload. Aids in unreliable connections.");

				if (config::EnableBackfill)
				{
					ImGui::SliderInt("Number of Past Input Frames", (int*)&config::NumBackFrames.get(), 1, 40);
					ImGui::SameLine();
					ShowHelpMarker("Number of past inputs to send per frame.");
				}
			}
		}

		ImGui::PopStyleVar();
		ImGui::EndTabItem();
	}
}

void DojoGui::update_action()
{
	if (ImGui::BeginPopupModal("Update?", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiInputTextFlags_EnterReturnsTrue))
	{
		std::string tag_name;
		std::string download_url;

		std::tie(tag_name, download_url) = dojo_file.tag_download;

		if (strcmp(tag_name.data(), GIT_VERSION) != 0)
		{
			ImGui::Text("There is a new version of Flycast Dojo available.\nWould you like to Update?");

			if (ImGui::BeginPopupModal("Update", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiInputTextFlags_EnterReturnsTrue))
			{
				ImGui::Text(dojo_file.status_text.data());
				if (strcmp(dojo_file.status_text.data(), "Update complete.\nPlease restart Flycast Dojo to use new version.") == 0)
				{
					if (ImGui::Button("Exit"))
					{
						exit(0);
					}
				}
				else
				{
					float progress 	= float(dojo_file.downloaded_size) / float(dojo_file.total_size);
					char buf[32];
					sprintf(buf, "%d/%d", (int)(progress * dojo_file.total_size), dojo_file.total_size);
					ImGui::ProgressBar(progress, ImVec2(0.f, 0.f), buf);
				}
				ImGui::EndPopup();
			}

			if (ImGui::Button("Update"))
			{
				ImGui::OpenPopup("Update");
				dojo_file.start_update = true;
			}
			ImGui::SameLine();
		}
		else
		{
			ImGui::Text("Flycast Dojo is already on the newest version.");
		}

		if (ImGui::Button("Close"))
		{
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}

	if (dojo_file.start_update && !dojo_file.update_started)
	{
		std::thread t([&]() {
			dojo_file.Update();
		});
		t.detach();
	}

}

DojoGui dojo_gui;

