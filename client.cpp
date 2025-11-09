#include "session.hpp"
#include "opcodes.hpp"

#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include <d3d11.h>
#include <tchar.h>

#include <vector>
#include <thread>
#include <mutex>
#include <atomic>

#pragma comment( lib, "ws2_32.lib" )
#pragma comment( lib, "d3d11.lib" )
#pragma comment( lib, "d3dcompiler.lib" )

/*
   global directx11 data
*/
static ID3D11Device* g_pd3d_device{ nullptr };
static ID3D11DeviceContext* g_pd3d_device_context{ nullptr };
static IDXGISwapChain* g_pswap_chain{ nullptr };
static ID3D11RenderTargetView* g_pmain_render_target_view{ nullptr };

/*
   global application data
*/
static std::vector<game_info_t> g_game_list{ };
static std::mutex g_game_list_mutex{ };
static std::atomic<bool> g_connected{ false };
static std::atomic<bool> g_connecting{ false };
static session_t* g_session{ nullptr };
static int g_selected_game_id{ -1 };

/*
   forward declarations
*/
auto create_device_d3d( HWND hwnd ) -> bool;
auto cleanup_device_d3d( ) -> void;
auto create_render_target( ) -> void;
auto cleanup_render_target( ) -> void;
auto network_thread( ) -> void;
LRESULT WINAPI wnd_proc( HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam );

/**
 * @brief creates d3d11 device and swap chain
 * @param hwnd window handle
 * @return true if successful
 */
auto create_device_d3d( HWND hwnd ) -> bool
{
	DXGI_SWAP_CHAIN_DESC sd{ };
	sd.BufferCount = 2;
	sd.BufferDesc.Width = 200;
	sd.BufferDesc.Height = 200;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = hwnd;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = TRUE;
	sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	UINT create_device_flags{ 0 };
	D3D_FEATURE_LEVEL feature_level{ };
	const D3D_FEATURE_LEVEL feature_level_array[ 2 ]{ D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };

	HRESULT res{ D3D11CreateDeviceAndSwapChain(
		nullptr,
		D3D_DRIVER_TYPE_HARDWARE,
		nullptr,
		create_device_flags,
		feature_level_array,
		2,
		D3D11_SDK_VERSION,
		&sd,
		&g_pswap_chain,
		&g_pd3d_device,
		&feature_level,
		&g_pd3d_device_context ) };

	if ( res != S_OK )
		return false;

	create_render_target( );
	return true;
}

/**
 * @brief cleanup d3d11 device
 */
auto cleanup_device_d3d( ) -> void
{
	cleanup_render_target( );

	if ( g_pswap_chain )
	{
		g_pswap_chain->Release( );
		g_pswap_chain = nullptr;
	}

	if ( g_pd3d_device_context )
	{
		g_pd3d_device_context->Release( );
		g_pd3d_device_context = nullptr;
	}

	if ( g_pd3d_device )
	{
		g_pd3d_device->Release( );
		g_pd3d_device = nullptr;
	}
}

/**
 * @brief create render target view
 */
auto create_render_target( ) -> void
{
	ID3D11Texture2D* pback_buffer{ nullptr };
	g_pswap_chain->GetBuffer( 0, IID_PPV_ARGS( &pback_buffer ) );

	if ( pback_buffer )
	{
		g_pd3d_device->CreateRenderTargetView( pback_buffer, nullptr, &g_pmain_render_target_view );
		pback_buffer->Release( );
	}
}

/**
 * @brief cleanup render target view
 */
auto cleanup_render_target( ) -> void
{
	if ( g_pmain_render_target_view )
	{
		g_pmain_render_target_view->Release( );
		g_pmain_render_target_view = nullptr;
	}
}

/**
 * @brief network thread handling server communication
 */
auto network_thread( ) -> void
{
	/*
	   prevent multiple connections
	*/
	bool expected{ false };
	if ( !g_connecting.compare_exchange_strong( expected, true ) )
	{
		return; // already connecting or connected
	}

	WSADATA wsa{ };
	WSAStartup( MAKEWORD( 2, 2 ), &wsa );

	/*
	   create socket and connect
	*/
	socket_t sock{ socket( AF_INET, SOCK_STREAM, 0 ) };

	sockaddr_in addr{ };
	addr.sin_family = AF_INET;
	addr.sin_port = htons( 8888 );
	addr.sin_addr.s_addr = inet_addr( "127.0.0.1" );

	if ( connect( sock, reinterpret_cast<sockaddr*>( &addr ), sizeof( addr ) ) < 0 )
	{
		g_connecting = false;
		WSACleanup( );
		return;
	}

	g_session = new session_t{ sock };
	g_connected = true;

	/*
	   receive session key
	*/
	if ( !g_session->receive_key( ) )
	{
		g_connected = false;
		g_connecting = false;
		delete g_session;
		g_session = nullptr;
		WSACleanup( );
		return;
	}

	/*
	   receive game list
	*/
	packet_t game_list_pkt{ };
	if ( g_session->receive( game_list_pkt ) )
	{
		uint8_t opcode{ };
		game_list_pkt >> opcode;

		if ( opcode == static_cast<uint8_t>( opcode_t::GAME_LIST ) )
		{
			uint8_t count{ };
			game_list_pkt >> count;

			std::lock_guard<std::mutex> lock{ g_game_list_mutex };
			g_game_list.clear( );

			for ( uint8_t i{ 0 }; i < count; ++i )
			{
				game_info_t game{ };
				game_list_pkt >> game.name;

				uint8_t status{ };
				game_list_pkt >> status;
				game.status = static_cast<game_status_t>( status );

				game_list_pkt >> game.process_name;

				g_game_list.push_back( game );
			}
		}
	}

	/*
	   wait for disconnect or errors
	*/
	while ( g_connected )
	{
		std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
	}

	g_connecting = false;
	delete g_session;
	g_session = nullptr;
	WSACleanup( );
}

/**
 * @brief window procedure
 * @param hwnd window handle
 * @param msg message
 * @param wparam wparam
 * @param lparam lparam
 * @return result
 */
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler( HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam );
LRESULT WINAPI wnd_proc( HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam )
{
	if ( ImGui_ImplWin32_WndProcHandler( hwnd, msg, wparam, lparam ) )
		return true;

	switch ( msg )
	{
		case WM_SIZE:
			if ( g_pd3d_device != nullptr && wparam != SIZE_MINIMIZED )
			{
				cleanup_render_target( );
				g_pswap_chain->ResizeBuffers( 0, ( UINT )LOWORD( lparam ), ( UINT )HIWORD( lparam ), DXGI_FORMAT_UNKNOWN, 0 );
				create_render_target( );
			}
			return 0;

		case WM_SYSCOMMAND:
			if ( ( wparam & 0xfff0 ) == SC_KEYMENU )
				return 0;
			break;

		case WM_DESTROY:
			::PostQuitMessage( 0 );
			return 0;
	}

	return ::DefWindowProc( hwnd, msg, wparam, lparam );
}

/**
 * @brief main entry point
 * @return exit code
 */
auto main( ) -> int
{
	/*
	   create window without borders
	*/
	WNDCLASSEX wc{
		sizeof( WNDCLASSEX ),
		CS_CLASSDC,
		wnd_proc,
		0L,
		0L,
		GetModuleHandle( nullptr ),
		nullptr,
		nullptr,
		nullptr,
		nullptr,
		_T( "TinyLoader" ),
		nullptr };

	::RegisterClassEx( &wc );

	HWND hwnd{ ::CreateWindowEx(
		0,
		wc.lpszClassName,
		_T( "TinyLoader" ),
		WS_POPUP,
		100,
		100,
		200,
		200,
		nullptr,
		nullptr,
		wc.hInstance,
		nullptr ) };

	/*
	   initialize direct3d
	*/
	if ( !create_device_d3d( hwnd ) )
	{
		cleanup_device_d3d( );
		::UnregisterClass( wc.lpszClassName, wc.hInstance );
		return 1;
	}

	::ShowWindow( hwnd, SW_SHOWDEFAULT );
	::UpdateWindow( hwnd );

	/*
	   setup imgui context
	*/
	IMGUI_CHECKVERSION( );
	ImGui::CreateContext( );
	ImGuiIO& io{ ImGui::GetIO( ) };
	io.IniFilename = nullptr;

	/*
	   setup dark theme
	*/
	ImGui::StyleColorsDark( );

	/*
	   setup platform/renderer backends
	*/
	ImGui_ImplWin32_Init( hwnd );
	ImGui_ImplDX11_Init( g_pd3d_device, g_pd3d_device_context );

	/*
	   start network thread
	*/
	std::thread net_thread{ network_thread };
	net_thread.detach( );

	/*
	   main loop
	*/
	bool done{ false };
	while ( !done )
	{
		MSG msg{ };
		while ( ::PeekMessage( &msg, nullptr, 0U, 0U, PM_REMOVE ) )
		{
			::TranslateMessage( &msg );
			::DispatchMessage( &msg );

			if ( msg.message == WM_QUIT )
				done = true;
		}

		if ( done )
			break;

		/*
		   start imgui frame
		*/
		ImGui_ImplDX11_NewFrame( );
		ImGui_ImplWin32_NewFrame( );
		ImGui::NewFrame( );

		/*
		   create main window (movable, no resize)
		*/
		ImGui::SetNextWindowPos( ImVec2{ 0, 0 }, ImGuiCond_FirstUseEver );
		ImGui::SetNextWindowSize( ImVec2{ 200, 200 } );
		ImGui::Begin(
			"##main",
			nullptr,
			ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse );

		if ( !g_connected )
		{
			/*
			   center "Connecting..." text
			*/
			const char* text{ "Connecting..." };
			float text_width{ ImGui::CalcTextSize( text ).x };
			ImGui::SetCursorPosX( ( 200.0f - text_width ) * 0.5f );
			ImGui::SetCursorPosY( 100.0f - ImGui::GetTextLineHeight( ) * 0.5f );
			ImGui::Text( "%s", text );
		}
		else
		{
			/*
			   center "Game List" title
			*/
			const char* title{ "Game List" };
			float title_width{ ImGui::CalcTextSize( title ).x };
			ImGui::SetCursorPosX( ( 200.0f - title_width ) * 0.5f );
			ImGui::Text( "%s", title );
			ImGui::Separator( );

			/*
			   display game list table (centered)
			*/
			float table_width{ 180.0f };
			ImGui::SetCursorPosX( ( 200.0f - table_width ) * 0.5f );

			if ( ImGui::BeginTable( "games", 1, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg, ImVec2{ table_width, 0 } ) )
			{
				std::lock_guard<std::mutex> lock{ g_game_list_mutex };

				for ( size_t i{ 0 }; i < g_game_list.size( ); ++i )
				{
					const auto& game{ g_game_list[ i ] };

					ImGui::TableNextRow( );
					ImGui::TableNextColumn( );

					/*
					   disable selectable if offline or maintenance
					*/
					bool disabled{ game.status == game_status_t::OFFLINE || game.status == game_status_t::MAINTENANCE };

					if ( disabled )
						ImGui::PushStyleVar( ImGuiStyleVar_Alpha, 0.5f );

					bool is_selected{ g_selected_game_id == static_cast<int>( i ) };

					if ( ImGui::Selectable( game.name.c_str( ), is_selected, disabled ? ImGuiSelectableFlags_Disabled : 0 ) )
					{
						/*
						   single click to select
						*/
						if ( !disabled )
						{
							g_selected_game_id = static_cast<int>( i );
						}
					}

					if ( disabled )
						ImGui::PopStyleVar( );
				}

				ImGui::EndTable( );
			}

			/*
			   show "Load" button if a game is selected
			*/
			if ( g_selected_game_id >= 0 && g_selected_game_id < static_cast<int>( g_game_list.size( ) ) )
			{
				ImGui::Spacing( );

				/*
				   center "Load" button
				*/
				float button_width{ 60.0f };
				ImGui::SetCursorPosX( ( 200.0f - button_width ) * 0.5f );

				if ( ImGui::Button( "Load", ImVec2{ button_width, 0 } ) && g_session )
				{
					packet_t request{ };
					request << static_cast<uint8_t>( opcode_t::GAME_REQUEST );
					request << static_cast<uint8_t>( g_selected_game_id );
					g_session->send( request );
				}
			}
		}

		ImGui::End( );

		/*
		   rendering
		*/
		ImGui::Render( );
		const float clear_color[ 4 ]{ 0.0f, 0.0f, 0.0f, 1.0f };
		g_pd3d_device_context->OMSetRenderTargets( 1, &g_pmain_render_target_view, nullptr );
		g_pd3d_device_context->ClearRenderTargetView( g_pmain_render_target_view, clear_color );
		ImGui_ImplDX11_RenderDrawData( ImGui::GetDrawData( ) );

		g_pswap_chain->Present( 1, 0 );
	}

	/*
	   cleanup
	*/
	g_connected = false;

	ImGui_ImplDX11_Shutdown( );
	ImGui_ImplWin32_Shutdown( );
	ImGui::DestroyContext( );

	cleanup_device_d3d( );
	::DestroyWindow( hwnd );
	::UnregisterClass( wc.lpszClassName, wc.hInstance );

	return 0;
}
