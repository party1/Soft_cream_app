
// soft_cream_appDlg.cpp : 実装ファイル
//x21:実装

#include "stdafx.h"
#include "soft_cream_app.h"
#include "soft_cream_appDlg.h"
#include "afxdialogex.h"

//解放
#include "stdlib.h"
#include "stdio.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#include <process.h>
// process.hはマルチスレッドＡＰＩを使用するために必要
#include <math.h>
// sqrt()等のために必要
#include <time.h>
// CTime関連関数のために必要（？）
#include <mmsystem.h>
// timeGetTime()を使用するため
// プロジェクトの構成プロパティにて、リンカの追加依存ファイルとして
// winmm.lib　を別途指定しておく必要がある プロジェクト→プロパティ→リンカ→入力

#define WM_RCV (WM_USER+1)

#define MaxComPort	99
#define DefaultPort  4
#define X21_FREQ 100			// 100Hz
#define X21_PERIOD 0.01			// 100Hz Sampling Period
#define DISPLAY_REFRESH	5		// 数値で示されるサンプル数毎に画面を再描画する
#define AXES 3					// ３軸
#define PI 3.14159265358979

int RScomport = DefaultPort;	// default COM port = COM4
CString ParamFileName = _T("x21guitar.txt");	// Parameter File
CString DefaultDir = _T("D:\\");
CString CurrentDir = _T("D:\\");
HANDLE	hCOMnd = INVALID_HANDLE_VALUE;

OVERLAPPED rovl, wovl;		// 非同期通信用オーバーラップド構造体
unsigned char wbuf[1024];	// 非同期送信用データバッファ

HWND hDlg;		// メッセージ処理にて使用するためのダイアログウィンドウ自体のハンドル
HANDLE serialh;	// ワイヤレス受信スレッドのハンドル
HANDLE packeth;	// パラメータ設定プロトコル制御スレッドのハンドル

int rf;						// rf wireless communication switch
int datasize;				// received data count
int etime;					// elapsed time (sec)

#define MAXDATASIZE 1048576	// = 2^20
// 100Hz sampling : 10486sec = 2.9 hours...

#define PACKETSIZE 45		// Preamble(0x65), Seq(1byte), ax, ay, az, gx, gy, gz, temp (2bytes)
// e4x, e4y, e4z, lx, ly, lz, alpha (float : 4bytes), chksum(1byte)
// 1 + 1 + 2x7 + 4x7 + 1 = 2 + 14 + 28 + 1 = 45 bytes

#define PREAMBLE 0x65

int errcnt;		// パケット受信エラーの個数

// モーションセンサデータ用バッファメモリ
// 本来はalloc等にて動的にメモリを確保するべきだが、ここではグローバル変数としている
int accbuf[AXES][MAXDATASIZE], gyrobuf[AXES][MAXDATASIZE];
int seqbuf[MAXDATASIZE], tempbuf[MAXDATASIZE];

UINT timebuf[MAXDATASIZE];	// Windowsのシステム時間を記録するメモリ
double e4buf[AXES][MAXDATASIZE], laccbuf[AXES][MAXDATASIZE], alphabuf[MAXDATASIZE];

// 画面へのグラフ描画における設定パラメータ
#define INVALID_MODE 0				// 無効：Y軸 = 0となる直線が描画される
#define LINEAR_ACCELERATION_MODE 1	// リニア加速度
#define ATTITUDE_ANGLE_MODE 2		// 重力姿勢角e4　（-1 ～ +1の方向余弦表現）
#define ANGULAR_VELOCITY_MODE 3		// 角速度

#define X_AXIS 0
#define Y_AXIS 1
#define Z_AXIS 2


// グラフの縦軸感度を設定するパラメータ
// WIDE_RANGEを選択すると、グラフの縦軸範囲がセンサ及び計算結果（e4, linear acceleration）の
// ダイナミックレンジに一致する。また、MIDDLE -> NARROW　となるに従って縦軸の感度が上がる
#define NARROW_RANGE 1
#define MIDDLE_RANGE 2
#define WIDE_RANGE 3

int timescale;
int graph_enable;

static void CloseComPort(void)
{
	if (hCOMnd == INVALID_HANDLE_VALUE)
		return;
	CloseHandle(hCOMnd);
	hCOMnd = INVALID_HANDLE_VALUE;
}

static DWORD OpenComPort(int port)
{
	CString ComPortNum;
	COMMPROP	myComProp;
	DCB	myDCB;
	COMSTAT	myComStat;

	// 非同期ＩＯモードなのでタイムアウトは無効
	//	_COMMTIMEOUTS myTimeOut;

	if ((port < 0) || (port > MaxComPort))
		return -1;
	if (port < 10){
		ComPortNum.Format(_T("COM%d"), port);
	}
	else{
		ComPortNum.Format(_T("\\\\.\\COM%d"), port);	// Bill Gates' Magic ...
	}

	ZeroMemory(&rovl, sizeof(rovl));
	ZeroMemory(&wovl, sizeof(wovl));
	rovl.Offset = 0;
	wovl.Offset = 0;
	rovl.OffsetHigh = 0;
	wovl.OffsetHigh = 0;
	rovl.hEvent = NULL;
	wovl.hEvent = NULL;

	hCOMnd = CreateFile((LPCTSTR)ComPortNum, GENERIC_READ | GENERIC_WRITE, 0, NULL,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, NULL);
	// COMポートをオーバラップドモード（非同期通信モード）にてオープンしている

	if (hCOMnd == INVALID_HANDLE_VALUE){
		return -1;
	}

	GetCommProperties(hCOMnd, &myComProp);
	GetCommState(hCOMnd, &myDCB);
	//	if( myComProp.dwSettableBaud & BAUD_128000)
	//		myDCB.BaudRate = CBR_128000;
	//	else
	myDCB.BaudRate = CBR_115200;		// 115.2KbpsモードをWindowsAPIは正しく認識しない
	//	myDCB.BaudRate = CBR_9600;
	myDCB.fDtrControl = DTR_CONTROL_DISABLE;
	myDCB.Parity = NOPARITY;
	myDCB.ByteSize = 8;
	myDCB.StopBits = ONESTOPBIT;
	myDCB.fDsrSensitivity = FALSE;
	SetCommState(hCOMnd, &myDCB);
	DWORD	d;

	d = myComProp.dwMaxBaud;

	DWORD	myErrorMask;
	char	rbuf[32];
	DWORD	length;

	// オーバーラップドモードでは、タイムアウト値は意味をなさない

	//	GetCommTimeouts( hCOMnd, &myTimeOut);
	//	myTimeOut.ReadTotalTimeoutConstant = 10;	// 10 msec
	//	myTimeOut.ReadIntervalTimeout = 200;	// 200 msec
	//	SetCommTimeouts( hCOMnd, &myTimeOut);
	//	GetCommTimeouts( hCOMnd, &myTimeOut);
	//	ReadTimeOut = (int)myTimeOut.ReadTotalTimeoutConstant;

	ClearCommError(hCOMnd, &myErrorMask, &myComStat);

	if (myComStat.cbInQue > 0){
		int	cnt;
		cnt = (int)myComStat.cbInQue;
		for (int i = 0; i < cnt; i++){
			// Synchronous IO
			//			ReadFile( hCOMnd, rbuf, 1, &length, NULL);		
			//

			// オーバーラップドモードで、同期通信的なことを行うためのパッチコード
			ReadFile(hCOMnd, rbuf, 1, &length, &rovl);
			while (1){
				if (HasOverlappedIoCompleted(&rovl)) break;
			}
		}
	}

	return d;
}

#define RINGBUFSIZE 1024

// コールバック関数によるシリアル通信状況の通知処理

int rcomp;				// コールバック関数からの戻り値を記憶するためのグローバル変数

VOID CALLBACK FileIOCompletionRoutine(DWORD err, DWORD n, LPOVERLAPPED ovl)
{
	rcomp = n;			// 読み込んだバイト数をそのままグローバル変数に返す
}

// 無線パケットを受信するためのスレッド

unsigned __stdcall serialchk(VOID * dummy)
{
	DWORD myErrorMask;						// ClearCommError を使用するための変数
	COMSTAT	 myComStat;						// 受信バッファのデータバイト数を取得するために使用する
	unsigned char buf[1024];				// 無線パケットを受信するための一時バッファ

	unsigned char ringbuffer[RINGBUFSIZE];	// パケット解析用リングバッファ
	int rpos, wpos, rlen;					// リングバッファ用ポインタ（read, write)、データバイト数
	int rpos_tmp;							// データ解析用read位置ポインタ
	int rest;								// ClearCommErrorにて得られるデータバイト数を記録する

	int seq, expected_seq;					// モーションセンサから受信するシーケンス番号（８ビット）
	// と、その期待値（エラーがなければ受信する値）

	unsigned char packet[PACKETSIZE];		// パケット解析用バッファ
	int i, chksum;
	int iax, iay, iaz, igx, igy, igz;	// Interger motion data
	int temp;							// Temperature

	float *ftmp;
	UINT timestamp;						// time stamp from the system

	int count = 0;						// Postmessage Counter

	expected_seq = 0;
	rpos = wpos = 0;
	rlen = 0;

	while (rf){
		rcomp = 0;					// FileIOCompletionRoutineが返す受信バイト数をクリアしておく
		// まずは無線パケットの先頭の１バイトを読み出しに行く
		ReadFileEx(hCOMnd, buf, 1, &rovl, FileIOCompletionRoutine);

		while (1){
			SleepEx(100, TRUE);	// 最大で100ミリ秒 = 0.1秒の間スリープするが、その間に
			if (rcomp == 1) break;	// I/O完了コールバック関数が実行されるとスリープを解除する

			if (!rf){				// 外部プログラムからスレッドの停止を指示された時の処理
				CancelIo(hCOMnd);	// 発行済みのReadFileEx操作を取り消しておく
				break;
			}
		}

		if (!rf) break;				// 外部プログラムからスレッドの停止を指示された

		ringbuffer[wpos] = buf[0];
		wpos++;
		wpos = (wpos >= RINGBUFSIZE) ? 0 : wpos;
		rlen++;

		ClearCommError(hCOMnd, &myErrorMask, &myComStat);

		rest = myComStat.cbInQue;	// 受信バッファに入っているデータバイト数

		if (rest == 0) continue;		// 何も入っていなかった

		rcomp = 0;
		ReadFileEx(hCOMnd, buf, rest, &rovl, FileIOCompletionRoutine);
		SleepEx(16, TRUE);			// Windowsにおけるシリアルポートの標準レイテンシ（16msec）
		if (rcomp != rest){
			CancelIo(hCOMnd);		// ClearCommErrorで取得したデータバイト数に満たない
		}							// データしか受信されなかったので、先に発行したReadFileEx
		// をキャンセルしている（ほぼFatal errorに近い）

		i = 0;
		while (rcomp > 0){
			ringbuffer[wpos] = buf[i];	// リングバッファに受信データを転送する
			wpos++;
			wpos = (wpos >= RINGBUFSIZE) ? 0 : wpos;
			rlen++;
			i++;
			rcomp--;
		}

		// ここからパケット解析に入る

		while (1){					// 有効なパケットである限り解析を継続する
			while (rlen >= PACKETSIZE){		// 解析開始には少なくとも45バイトが必要になる
				if (ringbuffer[rpos] == PREAMBLE) break;
				rpos++;								// 先頭がPREAMBLEではなかった
				rpos = (rpos >= RINGBUFSIZE) ? 0 : rpos;
				rlen--;								// リングバッファから１バイト破棄する
			}
			if (rlen < PACKETSIZE) break;			// 最低限必要なリングバッファを使い果たした

			// rlenには、PREAMBLEを含めた全ての受信データのバイト数が入っている
			// PREAMBLE候補が受信されており、かつ後続の少なくともPACKETSIZE-1バイトが到着している

			rpos_tmp = rpos;	// リングバッファを検証するための仮ポインタ
			rpos_tmp++;			// packet length へのポインタになる
			rpos_tmp = (rpos_tmp >= RINGBUFSIZE) ? 0 : rpos_tmp;

			chksum = PREAMBLE;

			for (i = 0; i < (PACKETSIZE - 2); i++){
				packet[i] = ringbuffer[rpos_tmp];
				chksum += packet[i];
				rpos_tmp++;
				rpos_tmp = (rpos_tmp >= RINGBUFSIZE) ? 0 : rpos_tmp;
			}

			if ((chksum & 0xff) != ringbuffer[rpos_tmp]){	// チェックサムエラー
				rpos++;										// 先頭の１バイト = PREAMBLEを放棄する
				rpos = (rpos >= RINGBUFSIZE) ? 0 : rpos;
				rlen--;
				continue;	// 次のPREAMBLEを探しにいく
			}

			// PREAMBLE、チェックサムの値が正しいPACKETSIZ長のデータがpacket[]に入っている

			timestamp = timeGetTime();	// Windowsのシステム時間を取得する

			seq = packet[0];

			if (seq != expected_seq){
				errcnt += (seq + 256 - expected_seq) % 256;	// パケットエラー数を更新する
			}
			expected_seq = (seq + 1) % 256;

			temp = (packet[13] << 8) + packet[14];
			temp = (temp >= 32768) ? (temp - 65536) : temp;

			iax = (packet[1] << 8) + packet[2];		// 16ビットint型変数は、上位8ビット、下位8ビット
			iay = (packet[3] << 8) + packet[4];		// の順に送信されている
			iaz = (packet[5] << 8) + packet[6];
			igx = (packet[7] << 8) + packet[8];
			igy = (packet[9] << 8) + packet[10];
			igz = (packet[11] << 8) + packet[12];

			iax = (iax >= 32768) ? (iax - 65536) : iax;	// 符号なし16bit整数を符号付きに変換する演算
			iay = (iay >= 32768) ? (iay - 65536) : iay;
			iaz = (iaz >= 32768) ? (iaz - 65536) : iaz;
			igx = (igx >= 32768) ? (igx - 65536) : igx;
			igy = (igy >= 32768) ? (igy - 65536) : igy;
			igz = (igz >= 32768) ? (igz - 65536) : igz;

			accbuf[X_AXIS][datasize] = iax;
			accbuf[Y_AXIS][datasize] = iay;
			accbuf[Z_AXIS][datasize] = iaz;
			gyrobuf[X_AXIS][datasize] = igx;
			gyrobuf[Y_AXIS][datasize] = igy;
			gyrobuf[Z_AXIS][datasize] = igz;
			timebuf[datasize] = timestamp;
			tempbuf[datasize] = temp;
			seqbuf[datasize] = seq;

			ftmp = (float *)&packet[15];			// unsigned char型配列からfloat型変数（4バイト長）へ変換する
			e4buf[X_AXIS][datasize] = (double)*ftmp;

			ftmp = (float *)&packet[19];
			e4buf[Y_AXIS][datasize] = (double)*ftmp;

			ftmp = (float *)&packet[23];
			e4buf[Z_AXIS][datasize] = (double)*ftmp;

			ftmp = (float *)&packet[27];
			laccbuf[X_AXIS][datasize] = (double)*ftmp;

			ftmp = (float *)&packet[31];
			laccbuf[Y_AXIS][datasize] = (double)*ftmp;

			ftmp = (float *)&packet[35];
			laccbuf[Z_AXIS][datasize] = (double)*ftmp;

			ftmp = (float *)&packet[39];
			alphabuf[datasize] = (double)*ftmp;

			datasize++;
			if (datasize >= MAXDATASIZE){
				datasize = (MAXDATASIZE - 1);	// Almost fatal error...
			}

			count++;

			rpos = (rpos + PACKETSIZE) % RINGBUFSIZE;			// 正しく読み出せたデータをリングバッファから除去する
			rlen -= PACKETSIZE;								// バッファの残り容量を更新する

			etime = datasize / X21_FREQ;

			if (count == DISPLAY_REFRESH){			// DISPLAY_REFRESH = 10では0.1秒毎に描画する設定となっている
				PostMessage(hDlg, WM_RCV, (WPARAM)1, NULL);	// 必要に応じてメッセージを発生させる
				count = 0;
			}
		}
	}
	_endthreadex(0);	// スレッドを消滅させる
	return 0;
}

// アプリケーションのバージョン情報に使われる CAboutDlg ダイアログ

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// ダイアログ データ
	enum { IDD = IDD_ABOUTBOX };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV サポート

// 実装
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(CAboutDlg::IDD)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// Csoft_cream_appDlg ダイアログ



Csoft_cream_appDlg::Csoft_cream_appDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(Csoft_cream_appDlg::IDD, pParent)
	, m_xvRadio(0)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void Csoft_cream_appDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_GLVIEW, m_glView);
	DDX_Control(pDX, IDC_GLVIEW2, m_glView2);
	DDX_Control(pDX, IDC_EDIT1, msgED);
	//  DDX_Control(pDX, IDC_PICT0, IDC_PICT0);
	DDX_Radio(pDX, IDC_RADIO1, m_xvRadio);
	DDX_Control(pDX, IDC_ANIMATE1, m_xcAnimate_Remaining);
	//DDX_Control(pDX, IDC_ANIMATE2, m_xcAnimate_Result);
	DDX_Control(pDX, IDC_EDIT2, msgED1);
	DDX_Control(pDX, IDC_EDIT3, msgED2);
	DDX_Control(pDX, IDC_ANIMATE2, m_xcAnimate_Result);
	DDX_Control(pDX, IDC_EDIT4, msg_ED_G);
	DDX_Control(pDX, IDC_EDIT5, msg_ED_D);
	DDX_Control(pDX, IDC_EDIT6, msg_ED_T);
}

BEGIN_MESSAGE_MAP(Csoft_cream_appDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_MESSAGE(WM_RCV, &Csoft_cream_appDlg::OnMessageRCV)
	ON_BN_CLICKED(IDC_BUTTON1, &Csoft_cream_appDlg::OnBnClickedButton1)
	ON_BN_CLICKED(IDC_BUTTON2, &Csoft_cream_appDlg::OnBnClickedButton2)
	ON_BN_CLICKED(IDC_RADIO1, &Csoft_cream_appDlg::OnBnClickedRadio1)
	ON_BN_CLICKED(IDC_BUTTON3, &Csoft_cream_appDlg::OnBnClickedButton3)
	ON_WM_TIMER()
	ON_BN_CLICKED(IDC_BUTTON4, &Csoft_cream_appDlg::OnBnClickedButton4)
	ON_BN_CLICKED(IDC_RADIO2, &Csoft_cream_appDlg::OnBnClickedRadio2)
	ON_BN_CLICKED(IDC_RADIO3, &Csoft_cream_appDlg::OnBnClickedRadio3)
	ON_BN_CLICKED(IDOK, &Csoft_cream_appDlg::OnBnClickedOk)
	ON_BN_CLICKED(IDCANCEL, &Csoft_cream_appDlg::OnBnClickedCancel)
	ON_BN_CLICKED(IDC_RADIO4, &Csoft_cream_appDlg::OnBnClickedRadio4)
END_MESSAGE_MAP()


// Csoft_cream_appDlg メッセージ ハンドラー

BOOL Csoft_cream_appDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	hDlg = this->m_hWnd;

	// "バージョン情報..." メニューをシステム メニューに追加します。

	// IDM_ABOUTBOX は、システム コマンドの範囲内になければなりません。
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// このダイアログのアイコンを設定します。アプリケーションのメイン ウィンドウがダイアログでない場合、
	//  Framework は、この設定を自動的に行います。
	SetIcon(m_hIcon, TRUE);			// 大きいアイコンの設定
	SetIcon(m_hIcon, FALSE);		// 小さいアイコンの設定

	// TODO: 初期化をここに追加します。

	//パケット関連の変数初期化
	rf = 0;      
	//	pd = 0;
	datasize = 0;
	errcnt = 0;

	graph_enable = 1;

	CStdioFile pFile;
	CString buf;
	TCHAR pbuf[256], dirbuf[_MAX_PATH];
	int comport, dirlen;

	if (!pFile.Open(ParamFileName, CFile::modeRead)){
		msgED.SetWindowTextW(_T("設定ファイル x21guitar.txtが見つかりません"));
		RScomport = DefaultPort;
		dirlen = GetCurrentDirectory(_MAX_PATH, dirbuf);
		if (dirlen != 0){
			CurrentDir.Format(_T("%s\\"), dirbuf);
		}
	}
	else{
		pFile.ReadString(pbuf, 32);
		comport = ::_ttoi(pbuf);

		dirlen = GetCurrentDirectory(_MAX_PATH, dirbuf);

		if (dirlen != 0){
			CurrentDir.Format(_T("%s\\"), dirbuf);
		}

		CString cpath;
		pFile.ReadString(cpath);

		if (cpath.GetLength() != 0){
			DefaultDir = cpath;
		}

		if ((comport > 0) && (comport <= MaxComPort)){
			RScomport = comport;
			buf.Format(_T("通信ポートのポート番号は %d です"), comport);
			msgED.SetWindowTextW(buf);
		}
		else{
			buf.Format(_T("設定できないポート番号です [COM1-COM%d が有効です]"), MaxComPort);
			msgED.SetWindowTextW(buf);
			RScomport = DefaultPort;	// default port number
		}
		pFile.Close();
	}



	//Open_GLを初期化する

	m_pDC = new CClientDC(&m_glView);	// 描画用のデバイスコンテキスト
	if (SetUpPixelFormat(m_pDC->m_hDC) != FALSE){	// 描画可能であればOpenGLの初期化を続行する
		m_GLRC = wglCreateContext(m_pDC->m_hDC);
		wglMakeCurrent(m_pDC->m_hDC, m_GLRC);

		CRect rc;
		m_glView.GetClientRect(&rc);
		GLint width = rc.Width();
		GLint height = rc.Height();
		GLdouble aspect = (GLdouble)width / (GLdouble)height;

		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);		// 背景は真っ黒になる
		glViewport(0, 0, width, height);
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();

		//		glOrtho( -aspect, aspect, -1.0, 1.0, -10.0, 10.0);

		gluPerspective(30.0, aspect, 0.1, 100.0);	// 透視投影を行う

		// 視野角30.0度、アスペクト比は描画領域と同じ、描画範囲となるZの最小値は0.1, 最大値は100.0
		// TODO: ここにメッセージ ハンドラ コードを追加します。
		// 描画メッセージで CStatic::OnPaint() を呼び出さないでください。

		//		gluLookAt( 4.0, 4.0, 4.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0); 
		gluLookAt(20.0, 65.0,20.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0);	// 視点の位置を決める
		//8,25,8
		//20,50,20
		//0.70,0
		// 視点位置は (6 6 4)t, 視線の先は　(0 0 0)t, 上を表す方向ベクトルは (0 0 1)t


		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
	}
	// OpenGLの初期化はここまで

	//m_nTimer = SetTimer(1, 30, 0);//Timerセット　0.03秒
	//1/5　変更箇所
	//アニメーション初期化
	m_xcAnimate_Remaining.Open(L"ゲージ6R.avi");
	//ゲージ6
	m_xcAnimate_Remaining.Play(0, -1, 1);
	//SetTimer(2, 1000, 0);//Timerセット　1秒
	SetTimer(3, 30, 0);//Timerセット　0.03秒(本当はいらないけど都合により設置)
	//1/5　ここまで
	//以下フォントの大きさを変える
	CFont  *curFont;
	curFont = msgED.GetFont();
	LOGFONTW mylf;
	curFont->GetLogFont(&mylf);
	mylf.lfHeight = 20;
	mylf.lfWidth = 10;
	//mylf.lfWeight = FW_HEAVY;
	m_newFont = new CFont;
	m_newFont->CreateFontIndirectW(&mylf);
	msgED.SetFont(m_newFont);
	msgED1.SetFont(m_newFont);
	msgED2.SetFont(m_newFont);
	msg_ED_T.SetFont(m_newFont);

	srand((unsigned int)time(NULL));

	return TRUE;  // フォーカスをコントロールに設定した場合を除き、TRUE を返します。
}

void Csoft_cream_appDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// ダイアログに最小化ボタンを追加する場合、アイコンを描画するための
//  下のコードが必要です。ドキュメント/ビュー モデルを使う MFC アプリケーションの場合、
//  これは、Framework によって自動的に設定されます。
//コーンを描写する関数
void DrawCone(){
	//コーンの描画関数
	//8角形で上の面引いて構築
	//サイズ変更可能な関数化する？
	glColor3f(1.0, 1.0, 0.0);
	glBegin(GL_POLYGON);
	glVertex3d(0.0, 0.0, -8.0);
	glVertex3d(2.0, 0.0, 0.0);
	glVertex3d(1.75, 1.25, 0.0);
	glVertex3d(1.5, 1.5, 0.0);
	glVertex3d(1.25, 1.75, 0.0);
	glVertex3d(0.0, 2.0, 0.0);
	glVertex3d(-1.25, 1.75, 0.0);
	glVertex3d(-1.5, 1.5, 0.0);
	glVertex3d(-1.75, 1.25, 0.0);
	glVertex3d(-2.0, 0.0, 0.0);
	glVertex3d(-1.75, -1.25, 0.0);
	glVertex3d(-1.5, -1.5, 0.0);
	glVertex3d(-1.25, -1.75, 0.0);
	glVertex3d(0.0, -2.0, 0.0);
	glVertex3d(1.25, -1.75, 0.0);
	glVertex3d(1.5, -1.5, 0.0);
	glVertex3d(1.75, -1.25, 0.0);
	glVertex3d(2.0, 0.0, 0.0);
	glEnd();

	glColor3f(1.0, 0.0, 1.0);
	glBegin(GL_POLYGON);
	glVertex3d(2.0, 0.0, 0.0);
	glVertex3d(1.75, 1.25, 0.0);

	glVertex3d(1.5, 1.5, 0.0);
	glVertex3d(1.25, 1.75, 0.0);
	glVertex3d(0.0, 2.0, 0.0);
	glVertex3d(-1.25, 1.75, 0.0);
	glVertex3d(-1.5, 1.5, 0.0);
	glVertex3d(-1.75, 1.25, 0.0);
	glVertex3d(-2.0, 0.0, 0.0);
	glVertex3d(-1.75, -1.25, 0.0);
	glVertex3d(-1.5, -1.5, 0.0);
	glVertex3d(-1.25, -1.75, 0.0);
	glVertex3d(0.0, -2.0, 0.0);
	glVertex3d(1.25, -1.75, 0.0);
	glVertex3d(1.5, -1.5, 0.0);
	glVertex3d(1.75, -1.25, 0.0);
	glVertex3d(2.0, 0.0, 0.0);
	glEnd();
}

void DrawHideCone(){
	//コーン隠し用の壁の関数
	//4回置けば箱的な囲いに！
	glColor3f(1.0, 1.0, 1.0);
	glBegin(GL_POLYGON);
	glVertex3d(0.0, 0.0, 0.0);
	glVertex3d(6.0, 0.0, 0.0);
	glVertex3d(6.0, 0.0, 4.0);
	glVertex3d(0.0, 0.0, 4.0);
	glEnd();
}

void DrawHideCone(int stack){
	glTranslated(0.0, 0.0, stack*4.0);
	::glPushMatrix();
		::glPushMatrix();
		//3方面のコーン隠し
			glTranslated(-3.0, 3.0, 0.0);
			DrawHideCone();
			glRotated(-90, 0.0, 0.0, 1.0);
			DrawHideCone();
			glTranslated(0.0, 6.0, 0.0);
			DrawHideCone();
		::glPopMatrix();
		glTranslated(-3.0, -3.0, 0.0);
		DrawHideCone();
	::glPopMatrix();
}

void DrawAxis(){
	//xyzの3本線を引く関数
	glColor3f(1.0, 0.0, 0.0);
	glBegin(GL_LINES);
	glVertex3d(0.0, 0.0, 0.0);
	glVertex3d(18.0, 0.0, 0.0);

	glColor3f(0.0, 1.0, 0.0);
	glVertex3d(0.0, 0.0, 0.0);
	glVertex3d(0.0, 18.0, 0.0);

	glColor3f(0.0, 0.0, 1.0);
	glVertex3d(0.0, 0.0, 0.0);
	glVertex3d(0.0, 0.0, 18.0);
	glEnd();
}

void DrawServer(){
	//サーバー用の壁の関数
	//4回置けば箱
	//なんか変になったけど現時点では放置
	glColor3f(1.0, 1.0, 1.0);
	glBegin(GL_POLYGON);
	glVertex3d(0.0, 0.0, 0.0);
	glVertex3d(15.0, 0.0, 0.0);
	glVertex3d(15.0, 0.0, 8.0);
	glVertex3d(0.0, 0.0, 8.0);
	glEnd();
}

void DrawFallCream(){
	//落ちるクリームの関数
	//板
	glColor3f(0.0, 1.0, 1.0);
	glBegin(GL_POLYGON);
	glVertex3d(0.0, 0.0, 0.0);
	glVertex3d(1.0, 0.0, 0.0);
	glVertex3d(1.0, 0.0, 4.0);
	glVertex3d(0.0, 0.0, 4.0);
	glEnd();
}

//1/5　変更箇所
void DrawFallCream(int cream_color){
	//落ちるクリームの関数
	//円柱
	double pi = 3.1415;//円周率
	float radius = 1.0;//半径
	float height = 3.0;//高さ
	int sides = 16;//n角形

		glColor3f(0.0, 1.0, 0.0);
		//下面
		glNormal3d(0.0, -1.0, 0.0);
		glBegin(GL_POLYGON);
		for (double i = sides; i >= 0; --i) {
			double t = pi * 2 / sides * (double)i;
			glVertex3d(radius * cos(t), radius * sin(t), -height);
		}
		glEnd();


		//側面
		if (cream_color == 0){
			glColor3f(0.4, 0.2, 0.2);
		}
		else if (cream_color == 1){
			glColor3f(0.0, 0.5, 0.0);
		}
		else if (cream_color == 2){
			glColor3f(1.0, 0.0, 0.8);
		}
		else if (cream_color == 3){
			glColor3f(1.0, 1.0, 1.0);
		}
		glBegin(GL_QUAD_STRIP);
		for (double i = 0; i <= sides; i = i + 1){
			double t = i * 2 * pi / sides;
			glNormal3f((GLfloat)cos(t), 0.0, (GLfloat)sin(t));
			glVertex3f((GLfloat)(radius*cos(t)), (GLfloat)(radius*sin(t)), -height);
			glVertex3f((GLfloat)(radius*cos(t)), (GLfloat)(radius*sin(t)), height);
		}
		glEnd();

		//上面
		//glColor3f(1.0, 1.0, 1.0);
		glNormal3d(0.0, 1.0, 0.0);
		glBegin(GL_POLYGON);
		for (double i = 0; i < sides; i++) {
			double t = pi * 2 / sides * (double)i;
			glVertex3d(radius * cos(t), radius * sin(t), height);
		}
		glEnd();
}

void DrawFallCream(int cream_color, double fall_cream, int cream_number,int i){
	//色　落下位置　番号　for
	//引数4つで判別　番号と現在の落下位置で描画
	//条件から外れれば消滅するように
	if (cream_number == i + 1 && fall_cream >= -30 - 6 * i){
		DrawFallCream(cream_color);
	}
}
//1/5　ここまで

void Csoft_cream_appDlg::OnPaint()
{
	/*//1/5　変更箇所
	if (cream_count >= 150){
		//OnPaint側でアニメーション変化実験
		m_xcAnimate_Remaining.Open(L"246.avi");
		m_xcAnimate_Remaining.Play(0, -1, 1);
	}
	//1/5　ここまで
	*/
	CPaintDC dc(this); // device context for painting


	//1/7　変更箇所
	//座標ずれ修正


	::glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);
	glLineWidth(3.0);


	DrawAxis();//3本線を引く

	::glPushMatrix();
	//クリームの落下を描画
	if (hide_cone == 1){
		glTranslated(-7.5, 0.0, 15.0); 
		::glPushMatrix();
			glTranslated(7.5, 0.0, 0.0);
			if (fall_cream <= 0){
				glTranslated(0.0, 0.0, fall_cream);
				//1/5　変更箇所
				glRotated(cream_count+2, 0.0, 0.0, 1.0);
				for (int i = 0; i < 15; i++){
					cream_number[i] = i+1;
					glTranslated(0.0, 0.0, 6);
					DrawFallCream(cream_color, fall_cream, cream_number[i],i);
				}
				//1/5　ここまで
			}
		::glPopMatrix();
		//1/5　変更箇所
		//サーバーの描画
		DrawServer();
		glTranslated(-3.0, 3.0, 0.0);
		DrawServer();
		glRotated(-90, 0.0, 0.0, 1.0);
		DrawServer();
		glTranslated(0.0, 6.0, 0.0);
		DrawServer();
		//1/5　ここまで
	}
	::glPopMatrix();
	
	::glPushMatrix();
	glTranslated(0.0, 0.0, -8.0);

	//4
	//glRotated(90,1,1,0);

	//コーンを動かす
	glPushMatrix();
	glRotated(dx, 1.0, 0.0, 0.0);
	glRotated(dy, 0.0, 1.0, 0.0);
	glRotated(dz, 0.0, 0.0, 1.0);

	glPushMatrix();
	glScaled(1.5, 1.5, 1.5);
	//コーンの表示
	DrawCone();
	glPopMatrix();
	glPopMatrix();

	//関数化　一定ライン超えたら増えるように
	if (hide_cone == 1){
		DrawHideCone(0);
		if (cream_count > 250){
			DrawHideCone(1);
		}
		/*
		::glPushMatrix();
			::glPushMatrix();
				//3方面のコーン隠し
				glTranslated(-3.0, 3.0, 0.0);
				DrawHideCone();
				glRotated(-90, 0.0, 0.0, 1.0);
				DrawHideCone();
				glTranslated(0.0, 6.0, 0.0);
				DrawHideCone();
			::glPopMatrix();
			glTranslated(-3.0, -3.0, 0.0);
			DrawHideCone();
		::glPopMatrix();
		*/
	}

	::glPopMatrix();

	::glFinish();
	if (FALSE == ::SwapBuffers(m_pDC->GetSafeHdc())){}


	if (IsIconic())
	{
		CPaintDC dc(this); // 描画のデバイス コンテキスト

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// クライアントの四角形領域内の中央
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// アイコンの描画
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}

}



// ユーザーが最小化したウィンドウをドラッグしているときに表示するカーソルを取得するために、
//  システムがこの関数を呼び出します。
HCURSOR Csoft_cream_appDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

static double read_data(int mode, int axis, int range, int count)
// x21-e4が出力する様々な数値パラメータを統一的に扱うための関数
// このような方法では処理速度が犠牲となるものの、コードの保守性や可読性が
// 向上するため、Rapid Prototypingの手法にてしばしば用いられている
{
	double d;

	if ((axis < X_AXIS) || (axis > Z_AXIS)){
		return 0.0;	// invalid axis
	}

	switch (mode){
	case INVALID_MODE:
		d = 0.0;
		break;
	case LINEAR_ACCELERATION_MODE:
		d = laccbuf[axis][count];
		d *= (1.0 / 8.0);			// ±4Gが計測範囲であるとして、ダイナミックレンジは8Gとなる
		break;
	case ATTITUDE_ANGLE_MODE:
		d = e4buf[axis][count];
		d *= (1.0 / 2.0);			// e4（方向余弦表現）では値域は-1～+1であり、ダイナミックレンジは2となる
		break;
	case ANGULAR_VELOCITY_MODE:
		d = gyrobuf[axis][count];
		d *= (1.0 / 65536.0);		// 角速度信号は16ビット符号付き整数のため、ダイナミックレンジは約65536である
		break;					// 厳密にはダイナミックレンジは65535だが、恐らく大きな問題ではない
	default:
		d = 0.0;
		break;
	}

	switch (range){
	case WIDE_RANGE:
		break;
	case MIDDLE_RANGE:
		d *= 2.0;				// Middle Rangeでは縦軸が２倍に拡大される
		break;
	case NARROW_RANGE:
		d *= 5.0;				// Narrow Rangeでは縦軸は５倍に拡大される
		break;
	default:
		break;
	}

	return d;
}

LRESULT Csoft_cream_appDlg::OnMessageRCV(WPARAM wParam, LPARAM lParam)
// ワイヤレス受信スレッドよりメッセージにて駆動される画面描画関数
{
	int i, j, w, h, x, y, col;
	CString s;
	double dtmp, alpha;

	s.Format(_T("Count = %d Packet Error = %d\n"), etime, errcnt);
	msgED.SetWindowTextW(s);

	
	// 信頼度α用の表示領域を描画する
	CWnd* myPICT0 = GetDlgItem(IDC_PICT0);	// Statud Indicator
	CClientDC myPICTDC0(myPICT0);
	CRect myPICTRECT0;
	myPICT0->GetClientRect(myPICTRECT0);

	alpha = 0.0;
	for (i = 0; i < DISPLAY_REFRESH; i++){
		dtmp = alphabuf[datasize - i];
		alpha = (alpha < dtmp) ? dtmp : alpha;
	}
	// この時点でalphaにはDISPLAY_REFRESH期間内における信頼度係数αの最大値が入る
	col = (int)(alpha*10.0 + 0.5);	// １０段階に量子化する
	if (col > 8){	// αが0.9以上　＝　e4に十分な信頼性がある
		myPICTDC0.FillSolidRect(myPICTRECT0, RGB(0, 255, 0));			// グリーン
	}
	else{
		if (col > 3){	// αが0.4以上0.9未満　＝　e4にやや信頼性がある
			myPICTDC0.FillSolidRect(myPICTRECT0, RGB(255, 255, 0));	// イエロー
		}
		else{
			// αは0.3以下でほとんど信頼できない
			myPICTDC0.FillSolidRect(myPICTRECT0, RGB(255, 0, 0));		// レッド
		}
	}

	//--------------------------データの受け取りと変換-----------------------------//
	lx = laccbuf[X_AXIS][datasize-1];
	ly = laccbuf[Y_AXIS][datasize-1];

	e4x = e4buf[X_AXIS][datasize-1];
	e4y = e4buf[Y_AXIS][datasize-1];
	e4z = e4buf[Z_AXIS][datasize-1];

	/*
	//e4ベクトルをdegreeに変換
	double tmpx = asin(e4x)*(180.0 / PI);
	double tmpy = asin(e4y)*(180.0 / PI);
	double tmpz = asin(e4z)*(180.0 / PI);
	dx = tmpx;
	dy = tmpy;
	dz = tmpz;
	*/
	//--------------------------ここまで-------------------------------------//

	/*
	if (graph_enable == 0) return TRUE;	// グラフ描画のON/OFFを決める

	// グラフ描画領域のピクチャコントロールをリソースIDと対応付ける
	CWnd* myPICT1 = GetDlgItem(IDC_PICT1);	// CH1 display
	CWnd* myPICT2 = GetDlgItem(IDC_PICT2);	// CH2 display

	// デバイスコンテキストを作成
	CClientDC myPICTDC1(myPICT1);
	CClientDC myPICTDC2(myPICT2);

	// 描画領域を表す四角形（rectangle）の座標
	CRect myPICTRECT1, myPICTRECT2;

	// グラフを描画する
	int gsamples;

	gsamples = timescale*X21_FREQ;		// 描画に関与するサンプルの数
	// 全てのサンプルを描画させることになる

	// CH1 のグラフ描画

	myPICT1->GetClientRect(myPICTRECT1);
	w = myPICTRECT1.Width();
	h = myPICTRECT1.Height();

	myPICTDC1.FillSolidRect(myPICTRECT1, RGB(255, 255, 255));

	CPen myPEN01(PS_SOLID, 1, RGB(128, 255, 255));	// ペンを空色の点線に設定
	CPen* oldPEN01 = myPICTDC1.SelectObject(&myPEN01);
	// グラフに縦軸 = 0の基準線（水平線）を引く
	myPICTDC1.MoveTo(0, h >> 1);
	myPICTDC1.LineTo(w, h >> 1);
	myPICTDC1.SelectObject(oldPEN01);

	CPen myPEN1(PS_SOLID, 1, RGB(0, 0, 0));	// ペンを黒色に設定
	CPen* oldPEN1 = myPICTDC1.SelectObject(&myPEN1);

	if (datasize < gsamples){	// 受信データのサンプル数が画面幅分より少ない場合
		dy = read_data(ch1mode, ch1axis, ch1range, 0);
		y = (h >> 1) + (int)(dy*(double)h + 0.5);
		y = clipping(y, h);
		myPICTDC1.MoveTo(0, y);		// ペンを最初のデータが示す位置へ移動（Move）させる

		for (i = 1; i < datasize; i++){
			x = i*w / gsamples;
			dy = read_data(ch1mode, ch1axis, ch1range, i);
			y = (h >> 1) + (int)(dy*(double)h + 0.5);
			y = clipping(y, h);
			myPICTDC1.LineTo(x, y);		// ペンをデータが示す位置へと線を引きながら移動させる
		}
	}
	else{						// 画面幅分の受信データが既にある場合
		dy = read_data(ch1mode, ch1axis, ch1range, datasize - gsamples);
		y = (h >> 1) + (int)(dy*(double)h + 0.5);
		y = clipping(y, h);
		myPICTDC1.MoveTo(0, y);

		for (i = (datasize - gsamples + 1), j = 1; i < datasize; i++, j++){
			x = j*w / gsamples;
			dy = read_data(ch1mode, ch1axis, ch1range, i);
			y = (h >> 1) + (int)(dy*(double)h + 0.5);
			y = clipping(y, h);
			myPICTDC1.LineTo(x, y);		// ペンをデータが示す位置へと線を引きながら移動させる
		}
	}

	myPICTDC1.SelectObject(oldPEN1);	// 使ったペンを元の状態に戻しておく

	// CH2 のグラフ描画

	myPICT2->GetClientRect(myPICTRECT2);
	w = myPICTRECT2.Width();
	h = myPICTRECT2.Height();

	myPICTDC2.FillSolidRect(myPICTRECT2, RGB(255, 255, 255));

	CPen myPEN02(PS_SOLID, 1, RGB(255, 200, 200));	// ペンを薄赤色の点線に設定
	CPen* oldPEN02 = myPICTDC2.SelectObject(&myPEN02);
	// グラフに縦軸 = 0の基準線（水平線）を引く
	myPICTDC2.MoveTo(0, h >> 1);
	myPICTDC2.LineTo(w, h >> 1);
	myPICTDC2.SelectObject(oldPEN02);

	CPen myPEN2(PS_SOLID, 1, RGB(0, 0, 0));	// ペンを黒色に設定
	CPen* oldPEN2 = myPICTDC2.SelectObject(&myPEN2);

	if (datasize < gsamples){	// 受信データのサンプル数が画面幅分より少ない場合
		dy = read_data(ch2mode, ch2axis, ch2range, 0);
		y = (h >> 1) + (int)(dy*(double)h + 0.5);
		y = clipping(y, h);
		myPICTDC2.MoveTo(0, y);		// ペンを最初のデータが示す位置へ移動（Move）させる

		for (i = 1; i < datasize; i++){
			x = i*w / gsamples;
			dy = read_data(ch2mode, ch2axis, ch2range, i);
			y = (h >> 1) + (int)(dy*(double)h + 0.5);
			y = clipping(y, h);
			myPICTDC2.LineTo(x, y);		// ペンをデータが示す位置へと線を引きながら移動させる
		}
	}
	else{						// 画面幅分の受信データが既にある場合
		dy = read_data(ch2mode, ch2axis, ch2range, datasize - gsamples);
		y = (h >> 1) + (int)(dy*(double)h + 0.5);
		y = clipping(y, h);
		myPICTDC2.MoveTo(0, y);

		for (i = (datasize - gsamples + 1), j = 1; i < datasize; i++, j++){
			x = j*w / gsamples;
			dy = read_data(ch2mode, ch2axis, ch2range, i);
			y = (h >> 1) + (int)(dy*(double)h + 0.5);
			y = clipping(y, h);
			myPICTDC2.LineTo(x, y);		// ペンをデータが示す位置へと線を引きながら移動させる
		}
	}

	myPICTDC2.SelectObject(oldPEN2);	// 使ったペンを元の状態に戻しておく
	*/


#if 0
	// 文字をビットマップ表示する際のサンプルコード

	CString Rating;

	if (lx > ly){
		if (lx > lz){	// lx is the largest
			Rating = _T("X");
		}
		else{			// lz is the largest
			Rating = _T("Z");
		}
	}
	else{
		if (ly > lz){	// ly is the largest
			Rating = _T("Y");
		}
		else{			// lz is the largest
			Rating = _T("Z");
		}
	}

	CFont myFont, *oldFont;

	myFont.CreateFont(240, 160, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
		SHIFTJIS_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
		DEFAULT_PITCH | FF_DONTCARE, _T("Arial"));

	myPICTR->GetClientRect(myPICTRECTR);
	myPICTDCR.FillSolidRect(myPICTRECTR, RGB(255, 255, 255));
	oldFont = myPICTDCR.SelectObject(&myFont);
	myPICTDCR.TextOutW(40, 20, Rating);
	myPICTDCR.SelectObject(oldFont);

	CFont myFont2;

	myFont2.CreateFont(40, 20, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
		SHIFTJIS_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
		DEFAULT_PITCH | FF_DONTCARE, _T("Arial"));

	CString AlphaValue;

	AlphaValue.Format(_T("%7.5f"), alpha);

	myPICTA->GetClientRect(myPICTRECTA);
	myPICTDCA.FillSolidRect(myPICTRECTA, RGB(255, 255, 255));
	oldFont = myPICTDCA.SelectObject(&myFont2);
	myPICTDCA.TextOutW(0, 0, AlphaValue);
	myPICTDCA.SelectObject(oldFont);

#endif

	return TRUE;
}


// For OpenGL on MFC (added from the class view)
// クラスビューから手動にて追加したメソッド関数
// OpenGLにおける画面モードの設定を行っている
// 基本的にはこの関数の中身を変更することはない

BOOL Csoft_cream_appDlg::SetUpPixelFormat(HDC hdc)
{
	PIXELFORMATDESCRIPTOR pfd = {
		sizeof(PIXELFORMATDESCRIPTOR),
		1,
		PFD_DRAW_TO_WINDOW |
		PFD_SUPPORT_OPENGL |
		PFD_DOUBLEBUFFER,
		PFD_TYPE_RGBA,
		24,
		0, 0, 0, 0, 0, 0,
		0, 0,
		0, 0, 0, 0, 0,
		32,
		0,
		0,
		PFD_MAIN_PLANE,
		0,
		0, 0, 0
	};

	int pf = ChoosePixelFormat(hdc, &pfd);
	if (pf != 0) return SetPixelFormat(hdc, pf, &pfd);
	return false;
}

// For OpenGL on MFC (added from the class view
// ウィンドウが破棄される時に、描画用のリソースを開放する
// OnDestroy()メソッドはクラスビューよりメッセージハンドラ関数追加にて作成する
void Csoft_cream_appDlg::OnDestroy()
{
	CDialog::OnDestroy();

	wglMakeCurrent(NULL, NULL);
	wglDeleteContext(m_GLRC);
	delete m_pDC;
}


void Csoft_cream_appDlg::OnBnClickedButton1()
{
	// TODO: ここにコントロール通知ハンドラー コードを追加します。
	// Connect
	// ワイヤレス受信スレッドを起動する
	
	DWORD d;
	unsigned int tid;

	if (rf){
		msgED.SetWindowTextW(_T("すでに計測を行っています"));
		return;
	}

	d = OpenComPort(RScomport);

	if (d < 0){
		msgED.SetWindowTextW(_T("無線用の通信ポートを初期化できませんでした"));
		Invalidate();
		UpdateWindow();
		return;
	}

	if (datasize == 0){		// ファイルに書き出すべきデータが残っていない
		errcnt = 0;			// もしもdatasizeがゼロでなければ、計測が一旦中断された後に
		etime = 0;			// 再び実行されていると見なしている
	}

	rf = 1;

	serialh = (HANDLE)_beginthreadex(NULL, 0, serialchk, NULL, 0, &tid);

	if (serialh == NULL){
		rf = 0;
		CloseComPort();
		msgED.SetWindowTextW(_T("モーションの計測を実行できませんでした"));
		return;
	}

	msgED.SetWindowTextW(_T("計測を開始しました"));


	// モーションセンサデバイスへのパラメータ送信を行う際の拡張用コード
#if 0
	pd = 1;

	packeth = (HANDLE)_beginthreadex(NULL, 0, device_init, NULL, 0, &tid);
	// デバイスパラメータ設定用のスレッドを起動する

	if (packeth == NULL){
		rf = 0;
		pd = 0;
		CloseComPort();
		msgED.SetWindowTextW(_T("モーションの計測は実行できませんでした[2]"));
		return;
	}
#endif
}




void Csoft_cream_appDlg::OnBnClickedButton2()
{
	// TODO: ここにコントロール通知ハンドラー コードを追加します。
	// OFF
	// ワイヤレス受信スレッドを停止させる
	// ファイルへのデータ書き出しを行う際にもワイヤレス受信スレッドは停止するため、
	// このメソッドは一種の緊急停止用となっている

	if (rf == 0){
		msgED.SetWindowTextW(_T("計測は行われていませんでした"));
		return;
	}
	else{
		rf = 0;

		DWORD dwExitCode;
		while (1){
			GetExitCodeThread(serialh, &dwExitCode);
			if (dwExitCode != STILL_ACTIVE) break;
		}
		CloseHandle(serialh);
		serialh = NULL;
		CloseComPort();
		msgED.SetWindowTextW(_T("計測を停止しました"));
	}
}





//1/5　変更箇所　ほぼ全部
void Csoft_cream_appDlg::OnBnClickedButton3()
{
	// TODO: ここにコントロール通知ハンドラー コードを追加します。
	//プロトタイプモード作動ボタン
	//隠す壁を表示して描画を開始する
	if (hide_cone == 0){
		hide_cone = 1;
		SetTimer(1, 30, 0);//Timerセット　0.03秒
		//ボタン押した場合の初期
		m_xcAnimate_Remaining.Open(L"ゲージ6R.avi");
		m_xcAnimate_Remaining.Play(0, -1, 1);
		GetDlgItem(IDC_GLVIEW)->ShowWindow(SW_SHOW);
		OnPaint();
	}
}

//12/22 追加分 遠心力を計算して返す関数
double centrifugal_force_cal(double lx, double ly, double e4z)
{
	double cForce = 0;
	double judge = 1.0;
	//最初の一回は0を代入しないと最大値が1.0で保障されてしまうのでその修正
	if (lx == 0.0 && ly == 0.0){
		judge = 0.0;
	}
	// リニア加速度x,yの平方二乗和
	cForce = abs(sqrt((lx*lx) + (ly*ly))) + abs(sqrt(judge - (e4z*e4z)));

	return cForce;
}

//e4にてデバイスの傾きをdegreeで返す関数
double e4z_conversion_degree(double e4z)
{
	double deg = 0;
	//e4zの傾きをdegreeに直す
	deg = asin(e4z) * 180 / PI;

	return deg;
}


void Csoft_cream_appDlg::OnTimer(UINT_PTR nIDEvent)
{
	// TODO: ここにメッセージ ハンドラー コードを追加するか、既定の処理を呼び出します。
	double tmpx = asin(e4x)*(180.0 / PI);
	double tmpy = asin(e4y)*(180.0 / PI);
	double tmpz = asin(e4z)*(180.0 / PI);
	dx = -tmpx;
	dy = -tmpy;
	dz = -tmpz;
	
	double cenF = centrifugal_force_cal(lx, ly, e4z);
	double e4Deg = e4z_conversion_degree(e4z);

	//センサーが0点からずれているため最初の値を保存しておく
	if (firstCount == 0 && e4Deg != 0.0){
		saveDeg = e4Deg;
		firstCount++;
	}

	deterDeg = saveDeg - e4Deg;     //角度を最初の場所からどれだけ傾いたのかを直観的に分かる数値に直す



	//下に移動

	CString cf, ed;

	cf.Format(_T("%0.3f,\r\nMAX = %0.3f"), cenF, maxCen);
	ed.Format(_T("%0.3f,\r\nMAX = %0.3f"), deterDeg, maxDeg);
	msgED1.SetWindowTextW(cf);
	msgED2.SetWindowTextW(ed);
	/*
	cf.Format(_T("%f"), cenF);
	ed.Format(_T("%f"), e4Deg);
	msgED1.SetWindowTextW(cf);
	msgED2.SetWindowTextW(ed);
	*/
	//setTimerの回数毎に呼び出し
	//ID1は0.03秒毎
	//ID2は1秒毎

	if (hide_cone == 1){
		if (nIDEvent == 1){
			//条件を満たしている場合
			//アイスクリーム落下、実行カウント加算、描画
			//イチゴの場合のみ速度変化
			if (cream_color == 2){//イチゴ
				if (cream_count <= 400){//400まで
					if (((int)cream_count % 50) == 0){//50ごと
						if ((rand() % 3 + 1) == 1){//ランダム
							fall_cream_ch = 0.2;
							cream_count_ch = 0.8;
						}
						else if ((rand() % 3 + 1) == 2){
							fall_cream_ch = 0.7;
							cream_count_ch = 2.8;
						}
						else{
							fall_cream_ch = 0.5;
							cream_count_ch = 2;
						}
						//変更されたものを描画
						fall_cream -= fall_cream_ch;
						cream_count += cream_count_ch;
					}
					else{
						//50毎でない場合は設定された値で落下
						fall_cream -= fall_cream_ch;
						cream_count += cream_count_ch;
					}
				}
				else{
					if (cream_count <= 500){
						//400以降は変更なし
						fall_cream -= fall_cream_ch;
						cream_count += cream_count_ch;
					}
					else{
						//500以降は等速に
						fall_cream -= 0.5;
						cream_count += 2;
					}
					
				}
			}
			else{
				fall_cream -= 0.5;
				cream_count += 2;
			}
			OnPaint();

			//遠心力と傾きの最高点を保存しておく
			//こっちに移動
			if (cream_count <= 600&&maxCen < cenF){
				maxCen = cenF;

			}
			if (cream_count <= 600 && maxDeg < deterDeg){
				maxDeg = deterDeg;

			}
			//1/11　変更箇所

			if (cream_count <= 500){//500以下で50の倍数
				//クリームカウントでアニメーション変化テスト
				//double型にしたせいで元の方法が使えなくなったので仕方ないね
				//システム修復
				if (count_anim == 0&&cream_count >= 60){
					m_xcAnimate_Remaining.Open(L"ゲージ5R.avi");
					m_xcAnimate_Remaining.Play(0, -1, 1);
					count_anim++;
				}
				else if (count_anim == 1 && cream_count >= 120){
					m_xcAnimate_Remaining.Open(L"ゲージ4R.avi");
					m_xcAnimate_Remaining.Play(0, -1, 1);
					count_anim++;
				}
				else if (count_anim == 2 && cream_count >= 180){
					m_xcAnimate_Remaining.Open(L"ゲージ3R.avi");
					m_xcAnimate_Remaining.Play(0, -1, 1);
					count_anim++;
				}
				else if (count_anim == 3 && cream_count >= 240){
					m_xcAnimate_Remaining.Open(L"ゲージ2R.avi");
					m_xcAnimate_Remaining.Play(0, -1, 1);
					count_anim++;
				}
				else if (count_anim == 4 && cream_count >= 300){
					m_xcAnimate_Remaining.Open(L"ゲージ1R.avi");
					m_xcAnimate_Remaining.Play(0, -1, 1);
					count_anim++;
				}
				else if (count_anim == 5 && cream_count >= 360){
					m_xcAnimate_Remaining.Open(L"ゲージ0R.avi");
					m_xcAnimate_Remaining.Play(0, -1, 1);
					count_anim++;
				}
				else{
					//m_xcAnimate_Remaining.Open(L"0m.avi");
					//m_xcAnimate_Remaining.Play(0, -1, 1);
				}

			}

			//1/11　ここまで
			//maxCen = 1.0;//仮
			//maxDeg = 1.0;//同上

			if (cream_count > 600 ){//1/7　変更箇所 ゲームオーバー除外仮配置
				//|| maxCen >= cenF_border || maxDeg >= e4Deg_border
				//(cream_count > 300 && maxCen<cenF_no_move&&maxDeg<e4Deg_no_move)
				GetDlgItem(IDC_GLVIEW)->ShowWindow(SW_HIDE);
				GetDlgItem(IDC_ANIMATE2)->ShowWindow(SW_SHOW);
				//1/7　変更箇所
				//結果画面の表示
				GetDlgItem(IDC_STATIC_1)->ShowWindow(SW_SHOW);
				GetDlgItem(IDC_STATIC_2)->ShowWindow(SW_SHOW);
				GetDlgItem(IDC_STATIC_3)->ShowWindow(SW_SHOW);
				GetDlgItem(IDC_STATIC_4)->ShowWindow(SW_SHOW);
				GetDlgItem(IDC_STATIC_5)->ShowWindow(SW_SHOW);
				GetDlgItem(IDC_EDIT4)->ShowWindow(SW_SHOW);
				GetDlgItem(IDC_EDIT5)->ShowWindow(SW_SHOW);
				GetDlgItem(IDC_EDIT6)->ShowWindow(SW_SHOW);
				//1/7　ここまで
				//m_xcAnimate_Result.Open(L"250-400 (2).avi");
				//m_xcAnimate_Result.Play(0, -1, 1);

				//1/11　変更箇所
				//結果判定

				//実行時における最大の値を表示　本番は↓二つを消去
				//maxCen = 1.0;//仮
				//maxDeg = 1.0;//同上

				CString cf_MAX, ed_MAX;

				cf_MAX.Format(_T("%f"), maxCen);
				ed_MAX.Format(_T("%f"), maxDeg);
				msg_ED_G.SetWindowTextW(cf_MAX);
				msg_ED_D.SetWindowTextW(ed_MAX);

				if (cream_color == 3){
					cenF_border = cenF_border_hard;
					e4Deg_border = e4Deg_border_hard;
				}
				else{
					cenF_border = cenF_border_easy;
					e4Deg_border = e4Deg_border_easy;
				}


				if (maxCen >= cenF_border || maxDeg >= e4Deg_border){
					result_text = _T("残念ながら崩れました\r\n");
					if (maxCen >= cenF_border){//遠心力が高い
						result_text += _T("強く回しすぎのようです\r\n");
						if (cream_color == 0){
							m_xcAnimate_Result.Open(L"チョコ遠心力R.avi");
							m_xcAnimate_Result.Play(0, -1, 1);
						}
						else if (cream_color == 1){
							m_xcAnimate_Result.Open(L"抹茶遠心力R.avi");
							m_xcAnimate_Result.Play(0, -1, 1);
						}
						else if (cream_color == 2){
							m_xcAnimate_Result.Open(L"イチゴ遠心力R.avi");
							m_xcAnimate_Result.Play(0, -1, 1);
						}
						else if (cream_color == 3){
							m_xcAnimate_Result.Open(L"ノーマル遠心力R.avi");
							m_xcAnimate_Result.Play(0, -1, 1);
						}
						
					}
					else{//傾きすぎ
						result_text += _T("傾けすぎたみたいです\r\n");
						if (cream_color == 0){
							m_xcAnimate_Result.Open(L"チョコ傾きR.avi");
							m_xcAnimate_Result.Play(0, -1, 1);
						}
						else if (cream_color == 1){
							m_xcAnimate_Result.Open(L"抹茶傾きR.avi");
							m_xcAnimate_Result.Play(0, -1, 1);
						}
						else if (cream_color == 2){
							m_xcAnimate_Result.Open(L"イチゴ傾きR.avi");
							m_xcAnimate_Result.Play(0, -1, 1);
						}
						else if (cream_color == 3){
							m_xcAnimate_Result.Open(L"ノーマル傾きR.avi");
							m_xcAnimate_Result.Play(0, -1, 1);
						}
					}
				}
				else if (maxCen < cenF_border && maxDeg < e4Deg_border){
					if (maxCen<cenF_no_move){
						//maxDeg<e4Deg_no_move
						result_text = _T("ちゃんと回しましょう！\r\n");
						if (cream_color == 0){
							//m_xcAnimate_Result.Open(L"480760.avi");
							m_xcAnimate_Result.Open(L"チョコ溢れR.avi");
							m_xcAnimate_Result.Play(0, -1, 1);
						}
						else if (cream_color == 1){
							m_xcAnimate_Result.Open(L"抹茶溢れR.avi");
							m_xcAnimate_Result.Play(0, -1, 1);
						}
						else if (cream_color == 2){
							m_xcAnimate_Result.Open(L"イチゴ溢れR.avi");
							m_xcAnimate_Result.Play(0, -1, 1);
						}
						else if (cream_color == 3){
							m_xcAnimate_Result.Open(L"ノーマル溢れR.avi");
							m_xcAnimate_Result.Play(0, -1, 1);
						}
					}
					else{
						result_text = _T("よくできました\r\n");
						if (cream_color == 0){
							m_xcAnimate_Result.Open(L"チョコ成功R.avi");
							m_xcAnimate_Result.Play(0, -1, 1);
						}
						else if (cream_color == 1){
							m_xcAnimate_Result.Open(L"抹茶成功R.avi");
							m_xcAnimate_Result.Play(0, -1, 1);
						}
						else if (cream_color == 2){
							m_xcAnimate_Result.Open(L"イチゴ成功R.avi");
							m_xcAnimate_Result.Play(0, -1, 1);
						}
						else if (cream_color == 3){
							m_xcAnimate_Result.Open(L"ノーマル成功R.avi");
							m_xcAnimate_Result.Play(0, -1, 1);
						}
					}
				}

				msg_ED_T.SetWindowTextW(result_text);
				
				KillTimer(1);
				//1/11　ここまで
			}
		}
	}
	CDialogEx::OnTimer(nIDEvent);
}


void Csoft_cream_appDlg::OnBnClickedButton4()
{
	// TODO: ここにコントロール通知ハンドラー コードを追加します。
	//全部をOFF　つまりリセット
	hide_cone = 0;
	fall_cream = 0;
	cream_count = 0;
	for(int i = 0; i < 100; i++){
		cream_number[i]=0;
	}
	GetDlgItem(IDC_GLVIEW)->ShowWindow(SW_SHOW);
	GetDlgItem(IDC_ANIMATE2)->ShowWindow(SW_HIDE);
	//1/7　変更箇所
	//結果画面の表示
	GetDlgItem(IDC_STATIC_1)->ShowWindow(SW_HIDE);
	GetDlgItem(IDC_STATIC_2)->ShowWindow(SW_HIDE);
	GetDlgItem(IDC_STATIC_3)->ShowWindow(SW_HIDE);
	GetDlgItem(IDC_STATIC_4)->ShowWindow(SW_HIDE);
	GetDlgItem(IDC_STATIC_5)->ShowWindow(SW_HIDE);
	GetDlgItem(IDC_EDIT4)->ShowWindow(SW_HIDE);
	GetDlgItem(IDC_EDIT5)->ShowWindow(SW_HIDE);
	GetDlgItem(IDC_EDIT6)->ShowWindow(SW_HIDE);
	count_anim = 0;
	//1/7　ここまで

	//1/11　変更箇所
	//追加した変数をリセット
	maxCen = 0;
	maxDeg = 0;
	fall_cream_ch = 0;
	cream_count_ch = 0;
	KillTimer(1);
	m_xcAnimate_Remaining.Open(L"ゲージ6R.avi");
	m_xcAnimate_Remaining.Play(0, -1, 1);
	deterDeg = 0.0;
	OnPaint();

	//2/22　変更箇所
	cenF_border = 0.0;
	e4Deg_border = 0.0;
}

void Csoft_cream_appDlg::OnBnClickedRadio1()
{
	// TODO: ここにコントロール通知ハンドラー コードを追加します。
	//ラジオボタン3つ
	//場合によっては普通にボタン3つになるが現時点ではこの方式を採用
	//起動していない場合に色を選択
	if (hide_cone == 0){
		cream_color = 0;
	}
	else{
		
	}
}

void Csoft_cream_appDlg::OnBnClickedRadio2()
{
	// TODO: ここにコントロール通知ハンドラー コードを追加します。
	if (hide_cone == 0){
		cream_color = 1;
	}
	else{
		
	}
}


void Csoft_cream_appDlg::OnBnClickedRadio3()
{

	// TODO: ここにコントロール通知ハンドラー コードを追加します。
	if (hide_cone == 0){
		cream_color = 2;
	}
	else{
	}
}

//2/22　変更箇所
//バニラを追加
void Csoft_cream_appDlg::OnBnClickedRadio4()
{
	// TODO: ここにコントロール通知ハンドラー コードを追加します。
	if (hide_cone == 0){
		cream_color = 3;
	}
	else{
	}
}
//2/22　ここまで

//1/11　変更箇所
//終了時にTimer停止を追加
void Csoft_cream_appDlg::OnBnClickedOk()
{
	// TODO: ここにコントロール通知ハンドラー コードを追加します。
	KillTimer(1);
	KillTimer(3);
	CDialogEx::OnOK();
}


void Csoft_cream_appDlg::OnBnClickedCancel()
{
	// TODO: ここにコントロール通知ハンドラー コードを追加します。
	KillTimer(1);
	KillTimer(3);
	CDialogEx::OnCancel();
}
//1/11　ここまで
