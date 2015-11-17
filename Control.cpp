/*----------------------------------------------------------
	DirectInputサンプル
「フォースフィードバック・ジョイスティック入力」
--------------------------------------------------------------*/

#define STRICT
#define DIRECTINPUT_VERSION 0x0800

#include <windows.h>
#include <crtdbg.h>

#pragma warning( disable : 4996 ) // 警告を一時的に無効にする 
#include <strsafe.h>
#pragma warning( default : 4996 ) 

#include <dinput.h>
#include <dxerr.h>
#include "resource.h"

#define SAFE_RELEASE(x)	{if(x){(x)->Release();x=NULL;}}

// 必要なライブラリをリンクする
#pragma comment( lib, "dinput8.lib" )
#pragma comment( lib, "dxerr.lib" )
#pragma comment( lib, "dxguid.lib" )

/*-------------------------------------------
	外部変数
--------------------------------------------*/
// アプリケーション情報
HINSTANCE hInstApp;								// インスタンス・ハンドル
HWND hwndApp;									// ウインドウ・ハンドル
WCHAR szAppName[] = L"DirectInput S4 Force Feedback Activation";	// アプリケーション名
bool g_bActive = true;							// ウインドウのアクティブ状態

// DirectInputの変数
LPDIRECTINPUT8 g_pDInput = NULL;					// DirectInput
LPDIRECTINPUTDEVICE8 g_pDIDevice = NULL;			// DirectInputデバイス
LPDIRECTINPUTEFFECT g_pDIEffect = NULL;				// エフェクト
DIDEVCAPS g_diDevCaps;								// ジョイスティックの能力
#define DIDEVICE_BUFFERSIZE	100						// デバイスに設定するバッファ・サイズ

// メッセージ関数の定義
LRESULT CALLBACK MainWndProc(HWND hWnd,UINT msg,UINT wParam,LONG lParam);

/*-------------------------------------------
	アプリケーション初期化
--------------------------------------------*/
bool InitApp(HINSTANCE hInst,int nCmdShow){
	hInstApp=hInst;

	// ウインドウ・クラスの登録
	WNDCLASS wndclass;
	wndclass.hCursor		= LoadCursor(NULL,IDC_ARROW);
	wndclass.hIcon			= LoadIcon(hInst, (LPCTSTR)IDI_ICON1);
	wndclass.lpszMenuName	= NULL;
	wndclass.lpszClassName	= szAppName;
	wndclass.hbrBackground	= (HBRUSH)(COLOR_WINDOW + 1);
	wndclass.hInstance		= hInst;
	wndclass.style			= CS_BYTEALIGNCLIENT|CS_VREDRAW|CS_HREDRAW;
	wndclass.lpfnWndProc	= (WNDPROC)MainWndProc;
	wndclass.cbClsExtra		= 0;
	wndclass.cbWndExtra		= 0;

	if(!RegisterClass(&wndclass))
		return false;

	// メイン・ウインドウ
	hwndApp = CreateWindowEx(0,szAppName,szAppName,
							WS_OVERLAPPEDWINDOW,
							CW_USEDEFAULT,CW_USEDEFAULT,640,480,
							(HWND)NULL,(HMENU)NULL,
							hInst,(LPSTR)NULL);
	ShowWindow(hwndApp,nCmdShow);
	UpdateWindow(hwndApp);

	return true;
}

/*-------------------------------------------
	DirectInput 初期化
---------------------------------------------*/
BOOL CALLBACK EnumJoysticksCallback(const DIDEVICEINSTANCE* pdidInstance, VOID* pContext);
BOOL CALLBACK EnumAxesCallback(LPCDIDEVICEOBJECTINSTANCE lpddoi, LPVOID pvRef);

bool InitDInput(void){
	HRESULT hr;

	// DirectInputの作成
	hr = DirectInput8Create(hInstApp, DIRECTINPUT_VERSION, 
							IID_IDirectInput8, (void**)&g_pDInput, NULL); 
	if (FAILED(hr)) {
		DXTRACE_ERR(L"DirectInput8オブジェクトの作成に失敗", hr);
		return false;
	}

	// デバイスを列挙して作成
	hr = g_pDInput->EnumDevices(DI8DEVCLASS_GAMECTRL, EnumJoysticksCallback,
							NULL, DIEDFL_FORCEFEEDBACK | DIEDFL_ATTACHEDONLY);
	if (FAILED(hr) || g_pDIDevice==NULL)	{
		DXTRACE_ERR(L"DirectInputDevice8オブジェクトの作成に失敗。フォースフィードバックのジョイスティックが見付からない", hr);
		return false;
	}
 
	// データ形式を設定
	hr = g_pDIDevice->SetDataFormat(&c_dfDIJoystick2);
	if (FAILED(hr))	{
		DXTRACE_ERR(L"c_dfDIJoystick2形式の設定に失敗", hr);
		return false;
	}

	//モードを設定（フォアグラウンド＆排他モード）
	hr = g_pDIDevice->SetCooperativeLevel(hwndApp, DISCL_EXCLUSIVE | DISCL_FOREGROUND);
	if (FAILED(hr))	{
		DXTRACE_ERR(L"フォアグラウンド＆非排他モードの設定に失敗", hr);
		return false;
	}

	// 自動センタリングを無効にする(フォースフィードバックに必要)
	DIPROPDWORD DIPropAutoCenter;
	DIPropAutoCenter.diph.dwSize       = sizeof(DIPropAutoCenter);
	DIPropAutoCenter.diph.dwHeaderSize = sizeof(DIPROPHEADER);
	DIPropAutoCenter.diph.dwObj        = 0;
	DIPropAutoCenter.diph.dwHow        = DIPH_DEVICE;
	DIPropAutoCenter.dwData            = DIPROPAUTOCENTER_OFF;

	hr = g_pDIDevice->SetProperty(DIPROP_AUTOCENTER, &DIPropAutoCenter.diph);
	if (FAILED(hr))	{
		DXTRACE_ERR(L"自動センタリングの設定に失敗", hr);
		return false;
	}

	// コールバック関数を使って各軸のモードを設定
	hr = g_pDIDevice->EnumObjects(EnumAxesCallback, NULL, DIDFT_AXIS);
	if (FAILED(hr))	{
		DXTRACE_ERR(L"軸モードの設定に失敗", hr);
		return false;
	}

	// 軸モードを設定（絶対値モードに設定。デフォルトなので必ずしも設定は必要ない）
	DIPROPDWORD diprop;
	diprop.diph.dwSize	= sizeof(diprop); 
	diprop.diph.dwHeaderSize	= sizeof(diprop.diph); 
	diprop.diph.dwObj	= 0;
	diprop.diph.dwHow	= DIPH_DEVICE;
	diprop.dwData		= DIPROPAXISMODE_ABS;
//	diprop.dwData		= DIPROPAXISMODE_REL;	// 相対値モードの場合
	hr = g_pDIDevice->SetProperty(DIPROP_AXISMODE, &diprop.diph);
	if (FAILED(hr))	{
		DXTRACE_ERR(L"軸モードの設定に失敗", hr);
		return false;
	}

	// バッファリング・データを取得するため、バッファ・サイズを設定
	diprop.dwData = DIDEVICE_BUFFERSIZE;
	hr = g_pDIDevice->SetProperty(DIPROP_BUFFERSIZE, &diprop.diph);
	if (FAILED(hr))	{
		DXTRACE_ERR(L"バッファ・サイズの設定に失敗", hr);
		return false;
	}

	// 入力制御開始
	hr = g_pDIDevice->Acquire();
	if (FAILED(hr))	{
		DXTRACE_ERR(L"Acquireに失敗", hr);
		return false;
	}

	// ボタン０を押すと実行されるエフェクト・オブジェクトを作成
    DWORD           rgdwAxes[2]     = { DIJOFS_X, DIJOFS_Y };
    LONG            rglDirection[2] = { 0, 0 };

	DICONSTANTFORCE cf              = { 0 };

	cf.lMagnitude = 10000;					// Min -10000, MAX 100000

	// エフェクト作成：詳細
	DIEFFECT eff;
    ZeroMemory( &eff, sizeof(eff) );
    eff.dwSize                  = sizeof(DIEFFECT);
    eff.dwFlags                 = DIEFF_CARTESIAN | DIEFF_OBJECTOFFSETS;	// デカルト座標
    eff.dwDuration              = DI_SECONDS*2;								// 1回の持続時間 Def.:1000 msec = 1 sec.
    eff.dwSamplePeriod          = 0;										// エフェクトの再生間隔 ms
    eff.dwGain                  = DI_FFNOMINALMAX/2;						// ゲイン最大=DI_FFNOMIALMAX
    eff.dwTriggerButton         = DIEB_NOTRIGGER;							// トリガーなし
    eff.dwTriggerRepeatInterval = 0;										// トリガ時のエフェクト間隔 繰り返し抑制にはINFINITEにする
    eff.cAxes                   = 2;										//  エフェクトに関係するの軸の数 Def.:2
    eff.rgdwAxes                = rgdwAxes;
    eff.rglDirection            = rglDirection;
    eff.lpEnvelope              = NULL;
    eff.cbTypeSpecificParams    = sizeof(DICONSTANTFORCE);
    eff.lpvTypeSpecificParams   = &cf;
    eff.dwStartDelay            = 0;

	hr = g_pDIDevice->CreateEffect(	GUID_ConstantForce,
						&eff,			// エフェクトについて設定した構造体
						&g_pDIEffect,	// エフェクト・オブジェクトを受け取るポインタのポインタ
						NULL);			// 集合体なし
	if (FAILED(hr))	{
		DXTRACE_ERR(L"エフェクトを生成できません", hr);
		return false;
	}
	return true;
}

// ジョイスティックを列挙する関数
BOOL CALLBACK EnumJoysticksCallback(const DIDEVICEINSTANCE* pdidInstance, VOID* pContext){
	HRESULT hr;

	// 列挙されたジョイスティックへのインターフェイスを取得する。
	hr = g_pDInput->CreateDevice(pdidInstance->guidInstance, &g_pDIDevice, NULL);
	if (FAILED(hr)) 
		return DIENUM_CONTINUE;

	// ジョイスティックの能力を調べる
	g_diDevCaps.dwSize = sizeof(DIDEVCAPS);
	hr = g_pDIDevice->GetCapabilities(&g_diDevCaps);
	if (FAILED(hr))	{
		// ジョイスティック能力の取得に失敗
		SAFE_RELEASE(g_pDIDevice);
		return DIENUM_CONTINUE;
	}
	return DIENUM_STOP; // 1つ見付けたら列挙を止める
}

// ジョイスティックの軸を列挙する関数
BOOL CALLBACK EnumAxesCallback(LPCDIDEVICEOBJECTINSTANCE lpddoi, LPVOID pvRef){
	HRESULT hr;

	// 軸の値の範囲を設定（-1000～1000）
	DIPROPRANGE diprg;
	ZeroMemory(&diprg, sizeof(diprg));
	diprg.diph.dwSize	= sizeof(diprg); 
	diprg.diph.dwHeaderSize	= sizeof(diprg.diph); 
	diprg.diph.dwObj	= lpddoi->dwType;
	diprg.diph.dwHow	= DIPH_BYID;
	diprg.lMin	= -1000;
	diprg.lMax	= +1000;
	hr = g_pDIDevice->SetProperty(DIPROP_RANGE, &diprg.diph);
	if (FAILED(hr))
		return DIENUM_STOP;

	return DIENUM_CONTINUE;
}

/*-------------------------------------------
	DirectInputの解放処理
--------------------------------------------*/
bool ReleaseDInput(void){
	// エフェクトをデバイスからアンロードする
	if(g_pDIEffect)
		g_pDIEffect->Unload();

	// DirectInputのデバイスを解放
	SAFE_RELEASE(g_pDIEffect);
	if (g_pDIDevice)
		g_pDIDevice->Unacquire(); 
	SAFE_RELEASE(g_pDIDevice);
	SAFE_RELEASE(g_pDInput);

	return true;
}

/*-------------------------------------------
	終了の処理
--------------------------------------------*/
bool EndApp(void){	return true; }

/*-------------------------------------------
	キー入力処理
--------------------------------------------*/
void WM_KeyDown(HWND hWnd, UINT wParam){
	switch(wParam){
		// ウインドウを閉じる
		case VK_ESCAPE:
			DestroyWindow(hWnd);
			return;
	}
}

/*-------------------------------------------
	タイマー処理
--------------------------------------------*/
void WM_Timer(HWND hWnd, WPARAM wParam){	// 画面更新
	InvalidateRect(hWnd, NULL, TRUE);
}

/*-------------------------------------------
	ウインドウの描画処理
--------------------------------------------*/
void WM_Paint(HWND hWnd){
	PAINTSTRUCT ps;
	HDC hDC = BeginPaint(hWnd, &ps);

	if (g_pDIDevice!=NULL)	{
		HRESULT hr;
		WCHAR CData[1024];
		int y = 0;

		// デバイスの直接データを取得する
		hr = g_pDIDevice->Poll();
		if (FAILED(hr))		{
			hr = g_pDIDevice->Acquire();
			while (g_bActive && hr==DIERR_INPUTLOST)
				hr = g_pDIDevice->Acquire();
		}

		DIJOYSTATE2 dijs;
		hr = g_pDIDevice->GetDeviceState(sizeof(DIJOYSTATE2), &dijs);
		if (SUCCEEDED(hr))		{
			// Window 軸表示
			hr = StringCbPrintf(CData, sizeof(CData),
					L"[X=%d Y=%d Z=%d][Rx=%d Ry=%d Rz=%d][Slider1=%d Slider2=%d]",
					dijs.lX,	dijs.lY,	dijs.lZ,	dijs.lRx,	dijs.lRy,	dijs.lRz,	dijs.rglSlider[0],	dijs.rglSlider[1]);

			if (SUCCEEDED(hr))
				TextOut(hDC, 0, y, CData, lstrlen(CData));
			y += 20;

			// ボタン表示
			WCHAR buf[16];
			hr = StringCbCopy(CData, sizeof(CData), L"");
			for(int i=0; i<9 && SUCCEEDED(hr); i++)		{
				if(dijs.rgbButtons[i] & 0x80)
					hr = StringCbPrintf(buf, sizeof(buf), L"[%d]",i);
				else
					hr = StringCbCopy(buf, sizeof(buf), L"[-]");
				if (SUCCEEDED(hr))
					hr = StringCbCat(CData, sizeof(CData), buf);
			}
			if (SUCCEEDED(hr))
				TextOut(hDC, 0, y, CData, lstrlen(CData));
			y += 20;
		}
		else if (g_bActive && hr==DIERR_INPUTLOST)
			g_pDIDevice->Acquire();
		y += 40;

		// バッファリング・データを取得する
		while(g_bActive)	{
			DIDEVICEOBJECTDATA od;
			DWORD dwItems = 1;
			hr = g_pDIDevice->GetDeviceData(sizeof(DIDEVICEOBJECTDATA),
								&od, &dwItems, 0);
			if (hr==DIERR_INPUTLOST)
				g_pDIDevice->Acquire();
			else if (FAILED(hr) || dwItems == 0)
	            break;	// データが読めないか、存在しない
			else	{
				switch (od.dwOfs) 	{
				case DIJOFS_X:
					hr = StringCbPrintf(CData, sizeof(CData), L"[%lu] DIJOFS_X = %d", od.dwSequence, od.dwData); break;
				case DIJOFS_Y:
					hr = StringCbPrintf(CData, sizeof(CData), L"[%lu] DIJOFS_Y = %d", od.dwSequence, od.dwData); break;
				case DIJOFS_Z:
					hr = StringCbPrintf(CData, sizeof(CData), L"[%lu] DIJOFS_Z = %d", od.dwSequence, od.dwData); break;
				case DIJOFS_RX:
					hr = StringCbPrintf(CData, sizeof(CData), L"[%lu] DIJOFS_Rx = %d", od.dwSequence, od.dwData); break;
				case DIJOFS_RY:
					hr = StringCbPrintf(CData, sizeof(CData), L"[%lu] DIJOFS_Ry = %d", od.dwSequence, od.dwData); break;
				case DIJOFS_RZ:
					hr = StringCbPrintf(CData, sizeof(CData), L"[%lu] DIJOFS_Rz = %d", od.dwSequence, od.dwData); break;
				case DIJOFS_BUTTON0:
					if (od.dwData != 0)		{
						g_pDIEffect->Unload();
						g_pDIEffect->Start(1, 0);

					}
					hr = StringCbPrintf(CData, sizeof(CData), L"[%lu] DIJOFS_BUTTON0 = %d", od.dwSequence, od.dwData); break;
				case DIJOFS_BUTTON1:
					hr = StringCbPrintf(CData, sizeof(CData), L"[%lu] DIJOFS_BUTTON1 = %d", od.dwSequence, od.dwData); break;
				case DIJOFS_BUTTON2:
					hr = StringCbPrintf(CData, sizeof(CData), L"[%lu] DIJOFS_BUTTON2 = %d", od.dwSequence, od.dwData); break;
				case DIJOFS_BUTTON3:
					hr = StringCbPrintf(CData, sizeof(CData), L"[%lu] DIJOFS_BUTTON3 = %d", od.dwSequence, od.dwData); break;
				case DIJOFS_BUTTON4:
					hr = StringCbPrintf(CData, sizeof(CData), L"[%lu] DIJOFS_BUTTON4 = %d", od.dwSequence, od.dwData); break;
				case DIJOFS_BUTTON5:
					hr = StringCbPrintf(CData, sizeof(CData), L"[%lu] DIJOFS_BUTTON5 = %d", od.dwSequence, od.dwData); break;
				case DIJOFS_BUTTON6:
					hr = StringCbPrintf(CData, sizeof(CData), L"[%lu] DIJOFS_BUTTON6 = %d", od.dwSequence, od.dwData); break;
				case DIJOFS_BUTTON7:
					hr = StringCbPrintf(CData, sizeof(CData), L"[%lu] DIJOFS_BUTTON7 = %d", od.dwSequence, od.dwData); break;
				case DIJOFS_BUTTON8:
					hr = StringCbPrintf(CData, sizeof(CData), L"[%lu] DIJOFS_BUTTON8 = %d", od.dwSequence, od.dwData); break;
				case DIJOFS_SLIDER(0):
					hr = StringCbPrintf(CData, sizeof(CData), L"[%lu] DIJOFS_SLIDER(0) = %d", od.dwSequence, od.dwData); break;
				case DIJOFS_SLIDER(1):
					hr = StringCbPrintf(CData, sizeof(CData), L"[%lu] DIJOFS_SLIDER(1) = %d", od.dwSequence, od.dwData); break;
				default: 
					hr = StringCbPrintf(CData, sizeof(CData), L"[%lu] %ld = %d", od.dwSequence, od.dwOfs, od.dwData); break;
				}
				if (SUCCEEDED(hr))
					TextOut(hDC, 0, y, CData, lstrlen(CData));
				y += 20;
			}
		}
	}
	EndPaint(hWnd, &ps);
}

/*-------------------------------------------
	ウインドウ処理
--------------------------------------------*/
LRESULT CALLBACK MainWndProc(HWND hWnd,UINT msg,UINT wParam,LONG lParam){
	switch(msg)	{
		case WM_ACTIVATE:		// ウインドウのアクティブ状態が変化
			if(g_pDIDevice == NULL)
				break;
			g_bActive = wParam != WA_INACTIVE;
			if(wParam != WA_INACTIVE)
				g_pDIDevice->Acquire();
			break;
		case WM_LBUTTONDOWN:  // マウスが押された
			if (g_pDIEffect)
				g_pDIEffect->Start(1, 0);
			break;
		case WM_KEYDOWN:	// キーが押された
			WM_KeyDown(hWnd, wParam);
			break;
		case WM_CREATE:		// ウインドウが作られた
			SetTimer(hWnd, 1, 100, NULL);
			break;
		case WM_DESTROY:	// ウインドウが破棄される
			KillTimer(hWnd, 1);
			PostQuitMessage(0);
			break;
		case WM_TIMER:		// タイマー・イベント発生
			WM_Timer(hWnd, wParam);
			break;
		case WM_PAINT:		// 画面描画
			WM_Paint(hWnd);
			break;
		default:			// その他のメッセージ
			return DefWindowProc(hWnd,msg,wParam,lParam);
	}
	return 0L;
}

/*--------------------------------------------
	メッセージがないときの処理
---------------------------------------------*/
bool AppIdle(){
	return true;	// falseを返すとメッセージが来るまで待機する
}

/*--------------------------------------------
	メイン
---------------------------------------------*/
int WINAPI wWinMain(HINSTANCE hInst,HINSTANCE /*hPrevInst*/,LPWSTR /*lpCmdLine*/,int nCmdShow){
	// デバッグ ヒープ マネージャによるメモリ割り当ての追跡方法を設定
	_CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );

	bool flag = false;
	// アプリケーションに関する初期化
	if (InitApp(hInst,nCmdShow))	{
		// DirectInputに関する初期化
		flag = InitDInput();
	}

	// メッセージ・ループ
	MSG msg;
	msg.wParam = -1;
	while (flag)	{
		while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))		{
			if (msg.message == WM_QUIT)		{
				flag = false;
				break;
			}
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		if (AppIdle()==false)
			PostQuitMessage(0);
	}
	// DirectInputの終了処理
	ReleaseDInput();

	// アプリケーションの終了処理
	EndApp();
	return (int)msg.wParam;
}
