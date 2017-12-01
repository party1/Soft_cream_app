// PQ2016_GRAPHDlg.cpp : 実装ファイル
//

#include "stdafx.h"
#include "PQ2016_GRAPH.h"
#include "PQ2016_GRAPHDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#include <process.h>
// process.hはマルチスレッドＡＰＩを使用するために必要

#define WM_RCV (WM_USER+1)

#define MaxComPort	99
#define DefaultPort 4

int RScomport = DefaultPort;					// default COM port #4
CString ParamFileName = _T("PQ2016_GRAPH.txt");	// Parameter File
CString DefaultDir = _T("D:\\");				// データファイルを書き出す際のデフォルトディレクトリ
CString CurrentDir = _T("D:\\");

HANDLE	hCOMnd = INVALID_HANDLE_VALUE;			// 非同期シリアル通信用ファイルハンドル
OVERLAPPED rovl, wovl;							// 非同期シリアル通信用オーバーラップド構造体

HWND hDlg;										// ダイアログ自体のウィンドウハンドルを格納する変数
HANDLE serialh;									// 通信用スレッドのハンドル

int rf;											// 通信用スレッドの走行状態制御用変数
int datasize;									// 正しく受信したデータのサンプル数

#define MAXDATASIZE 1048576						// 適当に大きな数
												// 65536 x 16
												// 100Hzサンプリングで約６時間弱分

#define DATASORT 7			// データの種類は７種類
							// SEQ, Ax, Ay, Az, Gx, Gy, Gz, Temperature

#define PACKETSIZE 15		// ワイヤレス通信における１パケットあたりのデータバイト数
#define PREAMBLE 0x65		// パケットの先頭を示すデータ（プリアンブル）

#define SAMPLING 100		// サンプリング周波数 [Hz]

int databuf[DATASORT][MAXDATASIZE];				// 本当はこんなことをしてはいけない
int errcnt;										// 通信エラーの回数を数える変数

int xaxis;					// グラフ描画時の時間幅　（単位：秒）
int yaxis;					// グラブ描画を行う軸の番号（１～７）

static void CloseComPort(void)
{
	if( hCOMnd == INVALID_HANDLE_VALUE)
		return;
	CloseHandle( hCOMnd);
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

	if( (port < 0) || (port > MaxComPort))
		return -1;
	if( port < 10){
		ComPortNum.Format(_T("COM%d"), port);
	}else{
		ComPortNum.Format(_T("\\\\.\\COM%d"), port);	// Bill Gates' Magic ...
	}

	ZeroMemory( &rovl, sizeof(rovl));
	ZeroMemory( &wovl, sizeof(wovl));
	rovl.Offset = 0;
	wovl.Offset = 0;
	rovl.OffsetHigh = 0;
	wovl.OffsetHigh = 0;
	rovl.hEvent = NULL;
	wovl.hEvent = NULL;

	hCOMnd = CreateFile( (LPCTSTR)ComPortNum, GENERIC_READ | GENERIC_WRITE, 0, NULL,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, NULL);
	// COMポートをオーバラップドモード（非同期通信モード）にてオープンしている

	if( hCOMnd == INVALID_HANDLE_VALUE){
		return -1;
	}

	GetCommProperties( hCOMnd, &myComProp);
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

	ClearCommError( hCOMnd, &myErrorMask, &myComStat);

	if( myComStat.cbInQue > 0){
		int	cnt;
		cnt = (int)myComStat.cbInQue;
		for( int i = 0; i < cnt; i++){
// Synchronous IO
//			ReadFile( hCOMnd, rbuf, 1, &length, NULL);		
//

// オーバーラップドモードで、同期通信的なことを行うためのパッチコード
			ReadFile( hCOMnd, rbuf, 1, &length, &rovl);
			while(1){
				if( HasOverlappedIoCompleted(&rovl)) break;
			}
		}
	}

	return d;
}


// コールバック関数によるシリアル通信状況の通知処理

int rcomp;

VOID CALLBACK FileIOCompletionRoutine( DWORD err, DWORD n, LPOVERLAPPED ovl)
{
	rcomp = n;			// 読み込んだバイト数をそのままグローバル変数に返す
}

// 無線パケットを受信するためのスレッド

#define RINGBUFSIZE 1024		// 受信用リングバッファのサイズ（バイト数）

unsigned __stdcall serialchk( VOID * dummy)
{
	DWORD myErrorMask;						// ClearCommError を使用するための変数
	COMSTAT	 myComStat;						// 受信バッファのデータバイト数を取得するために使用する
	unsigned char buf[RINGBUFSIZE];			// 無線パケットを受信するための一時バッファ

	unsigned char ringbuffer[RINGBUFSIZE];	// パケット解析用リングバッファ
	int rpos, wpos, rlen;					// リングバッファ用ポインタ（read, write)、データバイト数
	int rpos_tmp;							// データ解析用read位置ポインタ
	int rest;								// ClearCommErrorにて得られるデータバイト数を記録する

	int seq, expected_seq;					// モーションセンサから受信するシーケンス番号（８ビット）
											// と、その期待値（エラーがなければ受信する値）

	unsigned char packet[PACKETSIZE];		// パケット解析用バッファ （１パケット分）
	int i, chksum, tmp;

	expected_seq = 0;						// 最初に受信されるべきSEQの値はゼロ
	rpos = wpos = 0;						// リングバッファの読み出し及び書き込みポインタを初期化
	rlen = 0;								// リングバッファに残っている有効なデータ数（バイト）

	while(rf){
		rcomp = 0;					// FileIOCompletionRoutineが返す受信バイト数をクリアしておく
		// まずは無線パケットの先頭の１バイトを読み出しに行く
		ReadFileEx( hCOMnd, buf, 1, &rovl, FileIOCompletionRoutine);

		while(1){
			SleepEx( 100, TRUE);	// 最大で100ミリ秒 = 0.1秒の間スリープするが、その間に
			if(rcomp == 1) break;	// I/O完了コールバック関数が実行されるとスリープを解除する

			if(!rf){				// 外部プログラムからスレッドの停止を指示された時の処理
				CancelIo(hCOMnd);	// 発行済みのReadFileEx操作を取り消しておく
				break;
			}
									// データが送られてこない時間帯では、受信スレッド内のこの部分
									// が延々と処理されているが、大半がSleepExの実行に費やされる
									// ことで、システムに与える負荷を軽減している。
		}

		if(!rf) break;				// 外部プログラムからスレッドの停止を指示された

		ringbuffer[wpos] = buf[0];	// 最初の１バイトが受信された
		wpos++;						// リングバッファの書き込み位置ポインタを更新
		wpos = (wpos >= RINGBUFSIZE)?0:wpos;	// １周していたらポインタをゼロに戻す（なのでRING）
		rlen++;						// リングバッファ内の有効なデータ数を＋１する

		ClearCommError( hCOMnd, &myErrorMask, &myComStat);	// 受信バッファの状況を調べるＡＰＩ

		rest = myComStat.cbInQue;	// 受信バッファに入っているデータバイト数が得られる

		if(rest == 0) continue;		// 何も入っていなかったので次の１バイトを待ちにいく

		rcomp = 0;
		ReadFileEx( hCOMnd, buf, rest, &rovl, FileIOCompletionRoutine);
									// 受信バッファに入っているデータを読み出す
									// 原理的にはrestで示される数のデータを受信することができるが、
									// 万一に備えてデータが不足してしまった時の処理を考える。

		SleepEx(16, TRUE);			// Windowsにおけるシリアルポートの標準レイテンシ（16msec）
		if( rcomp != rest){
			CancelIo(hCOMnd);		// ClearCommErrorで取得したデータバイト数に満たない
		}							// データしか受信されなかったので、先に発行したReadFileEx
									// をキャンセルしている

		i = 0;
		while(rcomp > 0){			// rcompには読み出すことのできたデータのバイト数が入っている
			ringbuffer[wpos] = buf[i];	// リングバッファに受信データを転送する
			wpos++;
			wpos = (wpos >= RINGBUFSIZE)?0:wpos;
			rlen++;
			i++;
			rcomp--;
		}

		// ここからパケット解析に入る

		while(1){					// 有効なパケットである限り解析を継続する
			while( rlen > 0){
				if( ringbuffer[rpos] == PREAMBLE) break;
				rpos++;								// 先頭がPREAMBLEではなかった
				rpos = (rpos >= RINGBUFSIZE)?0:rpos;
				rlen--;								// 有効なデータ数を１つ減らして再度先頭を調べる
			}

			if(rlen < PACKETSIZE) break;	// 解析に必要なデータバイト数に達していなかったので
											// 最初の１バイトを待つ処理に戻る

			rpos_tmp = rpos;	// リングバッファを検証するための仮ポインタ
								// まだリングバッファ上にあるデータが有効であると分かったわけではない

			for( i = 0, chksum = 0; i < (PACKETSIZE-1); i++){
				packet[i] = ringbuffer[rpos_tmp];	// とりあえず解析用バッファにデータを整列させる
				chksum += packet[i];
				rpos_tmp++;
				rpos_tmp = (rpos_tmp >= RINGBUFSIZE)?0:rpos_tmp;
			}

			if( (chksum & 0xff) != ringbuffer[rpos_tmp]){	// チェックサムエラーなのでパケットは無効
				rpos++;										// 先頭の１バイトを放棄する
				rpos = (rpos >= RINGBUFSIZE)?0:rpos;
				rlen--;
				continue;	// 次のPREAMBLEを探しにいく
			}

			// PREAMBLE、チェックサムの値が正しいPACKETSIZ長のデータがpacket[]に入っている

			seq = packet[1];
			databuf[0][datasize] = seq;

			if(seq != expected_seq){
				errcnt += (seq + 256 - expected_seq)%256;	// パケットエラー数を更新する
			}
			expected_seq = (seq + 1)%256;					// 次のseqの期待値をexpected_seqに入れる

			for( i = 0; i < 6; i++){	// 加速度３軸、角速度３軸、の６つのデータを読み出す
				tmp = (packet[i*2 + 2]<<8)&0xff00 | packet[i*2 + 3];	// この時点ではunsigned
				databuf[i+1][datasize] = (tmp > 32767)?(tmp - 65536):tmp;
			}

			datasize++;
			datasize = (datasize >= MAXDATASIZE)?(MAXDATASIZE-1):datasize;

			if( datasize%10 == 0){								// 10サンプルに1回の割合でメッセージを発生させる
				PostMessage( hDlg, WM_RCV, (WPARAM)1, NULL);	// 100Hzサンプリングであれば0.1秒に1回画面が更新される
			}													// PostMessage()自体では処理時間は極めて短い

			rpos = (rpos + PACKETSIZE)%RINGBUFSIZE;			// 正しく読み出せたデータをリングバッファから除去する
			rlen -= PACKETSIZE;								// バッファの残り容量を更新する
		}
	}
	_endthreadex(0);	// スレッドを消滅させる
	return 0;
}

// アプリケーションのバージョン情報に使われる CAboutDlg ダイアログ

class CAboutDlg : public CDialog
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

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
END_MESSAGE_MAP()


// CPQ2016_GRAPHDlg ダイアログ

CPQ2016_GRAPHDlg::CPQ2016_GRAPHDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CPQ2016_GRAPHDlg::IDD, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CPQ2016_GRAPHDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_EDIT1, msgED);
}

BEGIN_MESSAGE_MAP(CPQ2016_GRAPHDlg, CDialog)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	//}}AFX_MSG_MAP
	ON_BN_CLICKED(IDC_BUTTON1, &CPQ2016_GRAPHDlg::OnBnClickedButton1)
	ON_BN_CLICKED(IDC_BUTTON2, &CPQ2016_GRAPHDlg::OnBnClickedButton2)
	ON_BN_CLICKED(IDC_BUTTON3, &CPQ2016_GRAPHDlg::OnBnClickedButton3)
	ON_BN_CLICKED(IDC_RADIO1, &CPQ2016_GRAPHDlg::OnBnClickedRadio1)
	ON_BN_CLICKED(IDC_RADIO2, &CPQ2016_GRAPHDlg::OnBnClickedRadio2)
	ON_BN_CLICKED(IDC_RADIO3, &CPQ2016_GRAPHDlg::OnBnClickedRadio3)
	ON_BN_CLICKED(IDC_RADIO4, &CPQ2016_GRAPHDlg::OnBnClickedRadio4)
	ON_BN_CLICKED(IDC_RADIO5, &CPQ2016_GRAPHDlg::OnBnClickedRadio5)
	ON_BN_CLICKED(IDC_RADIO6, &CPQ2016_GRAPHDlg::OnBnClickedRadio6)
	ON_BN_CLICKED(IDC_RADIO8, &CPQ2016_GRAPHDlg::OnBnClickedRadio8)
	ON_BN_CLICKED(IDC_RADIO9, &CPQ2016_GRAPHDlg::OnBnClickedRadio9)
	ON_BN_CLICKED(IDC_RADIO10, &CPQ2016_GRAPHDlg::OnBnClickedRadio10)
	ON_MESSAGE( WM_RCV, &CPQ2016_GRAPHDlg::OnMessageRCV)
END_MESSAGE_MAP()


// CPQ2016_GRAPHDlg メッセージ ハンドラ

BOOL CPQ2016_GRAPHDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// "バージョン情報..." メニューをシステム メニューに追加します。
	hDlg = this->m_hWnd;		// このダイアログへのハンドルを取得する

	// IDM_ABOUTBOX は、システム コマンドの範囲内になければなりません。
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		CString strAboutMenu;
		strAboutMenu.LoadString(IDS_ABOUTBOX);
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

	CButton* radio1 = (CButton*)GetDlgItem(IDC_RADIO1);
	radio1->SetCheck(1);	// Y-axis : acc X
	yaxis = 1;				// 起動時にacc Xのボタンが押されている状態にする

	CButton* radio8 = (CButton*)GetDlgItem(IDC_RADIO8);
	radio8->SetCheck(1);	// X-axis : 2sec
	xaxis = 2;				// 起動時に2 secのボタンが押されている状態にする
	
	rf = 0;			// 無線パケット受信スレッドは停止
	datasize = 0;	// モーションデータの数を０に初期化


	// 設定ファイルを読み込み、シリアル通信用ポート番号とファイル保存先フォルダを設定する
	CStdioFile pFile;
	CString buf;
	char pbuf[256], dirbuf[_MAX_PATH];
	char pathname[256];
	int i, comport, dirlen;

	for( i = 0; i < 256; i++){
		pbuf[i] = pathname[i] = 0x00;	// 安全のためNULLで埋めておく
	}

	if(!pFile.Open( ParamFileName, CFile::modeRead)){
		msgED.SetWindowTextW(_T("Parameter file not found..."));
		RScomport = DefaultPort;
		dirlen = GetCurrentDirectory( _MAX_PATH, (LPTSTR)dirbuf);
		if( dirlen != 0){
		CurrentDir.Format(_T("%s\\"), dirbuf);
		}
	}else{
		pFile.ReadString((LPTSTR)pbuf, 32);
		dirlen = GetCurrentDirectory( _MAX_PATH, (LPTSTR)dirbuf);

		if( dirlen != 0){
		CurrentDir.Format(_T("%s\\"), dirbuf);
		}

		pbuf[1] = pbuf[2];	// Unicodeに対応するためのパッチコード
		pbuf[2] = 0x00;

		comport = atoi(pbuf);	// ポート番号をテキストファイルから取得している

		CString cpath;
		pFile.ReadString(cpath);

		if( cpath.GetLength() != 0){
			DefaultDir = cpath;
		}

		if( (comport > 0) && (comport <= MaxComPort)){
			RScomport = comport;
			buf.Format(_T("Parameter File Found. RS channel %d"), comport);
			msgED.SetWindowTextW(buf);
		}else{
			buf.Format(_T("Invalid Comport number! [COM1-COM%d are valid]"), MaxComPort);
			msgED.SetWindowTextW(buf);
			RScomport = DefaultPort;	// default port number
		}
		pFile.Close();
	}

	return TRUE;  // フォーカスをコントロールに設定した場合を除き、TRUE を返します。
}

void CPQ2016_GRAPHDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialog::OnSysCommand(nID, lParam);
	}
}

// ダイアログに最小化ボタンを追加する場合、アイコンを描画するための
//  下のコードが必要です。ドキュメント/ビュー モデルを使う MFC アプリケーションの場合、
//  これは、Framework によって自動的に設定されます。

void CPQ2016_GRAPHDlg::OnPaint()
{
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
		CDialog::OnPaint();
	}
}

// ユーザーが最小化したウィンドウをドラッグしているときに表示するカーソルを取得するために、
//  システムがこの関数を呼び出します。
HCURSOR CPQ2016_GRAPHDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

LRESULT CPQ2016_GRAPHDlg::OnMessageRCV( WPARAM wParam, LPARAM lParam)
{
	CString s;
	static int rcnt = 0;

	// 受信したパケット数と無線通信エラーの回数を表示する
	rcnt++;

	s.Format( _T("Received : %d Error = %d"), rcnt, errcnt);
	msgED.SetWindowTextW(s);


	// グラフを描画する

	CWnd* myPICT = GetDlgItem(IDC_GRAPH);	// 画面上のピクチャコントロールと対応付ける
	CClientDC myPICTDC(myPICT);				// デバイスコンテキストを作成
	CRect myPICTRECT;						// 描画領域を表す四角形（rectangle）の座標

	int i, j, x, y;
	int w, h;
	int gsamples;

	gsamples = xaxis*SAMPLING;		// 描画に関与するサンプルの数

	myPICT->GetClientRect(myPICTRECT);
	w = myPICTRECT.Width();
	h = myPICTRECT.Height();

	CPen myPEN( PS_SOLID, 1, RGB( 0, 0, 0));	// ペンを黒色に設定
	CPen* oldPEN = myPICTDC.SelectObject(&myPEN);

	myPICTDC.FillSolidRect( myPICTRECT, RGB( 255, 255, 255));

	if( datasize < gsamples){	// 受信データのサンプル数が画面幅分より少ない場合
		y = databuf[yaxis][0];
		y = ((y + 32768)*h)>>16;	// 縦軸の範囲は -32768 ～ +32768に設定している
		myPICTDC.MoveTo( 0, y);		// ペンを最初のデータが示す位置へ移動（Move）させる

		for( i = 1; i < datasize; i++){
			x = i*w/gsamples;
			y = databuf[yaxis][i];
			y = ((y + 32768)*h)>>16;	// 16ビット右シフトすることで655536で割っている
			myPICTDC.LineTo( x, y);		// ペンをデータが示す位置へと線を引きながら移動させる
		}
	}else{						// 画面幅分の受信データが既にある場合
		y = databuf[yaxis][datasize - gsamples];
		y = ((y + 32768)*h)>>16;
		myPICTDC.MoveTo( 0, y);

		for( i = (datasize - gsamples + 1), j = 1; i < datasize; i++, j++){
			x = j*w/gsamples;
			y = databuf[yaxis][i];
			y = ((y + 32768)*h)>>16;
			myPICTDC.LineTo( x, y);
		}
	}

	myPICTDC.SelectObject(oldPEN);	// 使ったペンを元の状態に戻しておく

	return TRUE;
}

void CPQ2016_GRAPHDlg::OnBnClickedButton1()
{
	// START

	DWORD d;

	if(rf){
		msgED.SetWindowTextW(_T("Thread is already running..."));
		return;
	}

	d = OpenComPort(RScomport);

	if( d < 0){
		msgED.SetWindowTextW(_T("can't initialize COM port"));
		Invalidate();
		UpdateWindow();
		return;
	}

	rf = 1;
	errcnt = 0;

	unsigned int tid;
	serialh = (HANDLE)_beginthreadex( NULL, 0, serialchk, NULL, 0, &tid);

	if(serialh != NULL){
		msgED.SetWindowTextW(_T("Start Recording"));
	}else{
		rf = 0;
		CloseComPort();
		msgED.SetWindowTextW(_T("Thread is not running..."));
	}
}

void CPQ2016_GRAPHDlg::OnBnClickedButton2()
{
	// STOP

	if(rf){
		rf = 0;

		DWORD dwExitCode;
		while(1){
			GetExitCodeThread( serialh, &dwExitCode);
			if( dwExitCode != STILL_ACTIVE) break;
		}
		CloseHandle(serialh);
		serialh = NULL;
		CloseComPort();
		msgED.SetWindowTextW(_T("Stop Recording"));
	}else{
		msgED.SetWindowTextW(_T("Recording is not running..."));
	}
}

void CPQ2016_GRAPHDlg::OnBnClickedButton3()
{
	// FILE

	DWORD		dwFlags;
	LPCTSTR		lpszFilter = _T("Text File(*.txt)|*.txt|");
	CString		fn, pathn;
	int			i, j, k;
	CString		rbuf;
	CString		writebuffer;

	if(rf){
		msgED.SetWindowTextW(_T("Recording thread is still running..."));
		return;
	}

	if(datasize == 0){
		msgED.SetWindowTextW(_T("There is no data..."));	
		return;
	}

	dwFlags = OFN_OVERWRITEPROMPT | OFN_CREATEPROMPT;

	CFileDialog myDLG( FALSE, _T("txt"), NULL, dwFlags, lpszFilter, NULL);
	myDLG.m_ofn.lpstrInitialDir = DefaultDir;


	if( myDLG.DoModal() != IDOK) return;


	CStdioFile fout(myDLG.GetPathName(), CFile::modeWrite | CFile::typeText | CFile::modeCreate);

	pathn = myDLG.GetPathName();
	fn = myDLG.GetFileName();
	j = pathn.GetLength();
	k = fn.GetLength();
	DefaultDir = pathn.Left(j-k);

	msgED.SetWindowTextW(_T("Writing the Data File..."));

	Invalidate();
	UpdateWindow();

	// CCP-X21GTR RAW DATA FORMAT
	for( i = 0; i < datasize; i++){
		writebuffer.Format(_T("%d\t%d\t%d\t%d\t%d\t%d\t%d\n"), 
			databuf[0][i], databuf[1][i], databuf[2][i], databuf[3][i], databuf[4][i], databuf[5][i], databuf[6][i]);
			// SEQ, Ax, Ay, Az, Gx, Gy, Gz 温度 lx ly lz e4x e4y e4z alpha　in integer
		fout.WriteString((LPCTSTR)writebuffer);
	}

	fout.Close();

	datasize = 0;

	msgED.SetWindowTextW(_T("Motion Data File Writing Succeeded"));
}

void CPQ2016_GRAPHDlg::OnBnClickedRadio1()
{
	// Acc X
	yaxis = 1;
}

void CPQ2016_GRAPHDlg::OnBnClickedRadio2()
{
	// Acc Y
	yaxis = 2;
}

void CPQ2016_GRAPHDlg::OnBnClickedRadio3()
{
	// Acc Z
	yaxis = 3;
}

void CPQ2016_GRAPHDlg::OnBnClickedRadio4()
{
	// Gyro X
	yaxis = 4;
}

void CPQ2016_GRAPHDlg::OnBnClickedRadio5()
{
	// Gyro Y
	yaxis = 5;
}

void CPQ2016_GRAPHDlg::OnBnClickedRadio6()
{
	// Gyro Z
	yaxis = 6;
}


void CPQ2016_GRAPHDlg::OnBnClickedRadio8()
{
	// 2 sec
	xaxis = 2;
}

void CPQ2016_GRAPHDlg::OnBnClickedRadio9()
{
	// 5 sec
	xaxis = 5;
}

void CPQ2016_GRAPHDlg::OnBnClickedRadio10()
{
	// 10 sec
	xaxis = 10;
}
