// PQ2016_GRAPHDlg.cpp : �����t�@�C��
//

#include "stdafx.h"
#include "PQ2016_GRAPH.h"
#include "PQ2016_GRAPHDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#include <process.h>
// process.h�̓}���`�X���b�h�`�o�h���g�p���邽�߂ɕK�v

#define WM_RCV (WM_USER+1)

#define MaxComPort	99
#define DefaultPort 4

int RScomport = DefaultPort;					// default COM port #4
CString ParamFileName = _T("PQ2016_GRAPH.txt");	// Parameter File
CString DefaultDir = _T("D:\\");				// �f�[�^�t�@�C���������o���ۂ̃f�t�H���g�f�B���N�g��
CString CurrentDir = _T("D:\\");

HANDLE	hCOMnd = INVALID_HANDLE_VALUE;			// �񓯊��V���A���ʐM�p�t�@�C���n���h��
OVERLAPPED rovl, wovl;							// �񓯊��V���A���ʐM�p�I�[�o�[���b�v�h�\����

HWND hDlg;										// �_�C�A���O���̂̃E�B���h�E�n���h�����i�[����ϐ�
HANDLE serialh;									// �ʐM�p�X���b�h�̃n���h��

int rf;											// �ʐM�p�X���b�h�̑��s��Ԑ���p�ϐ�
int datasize;									// ��������M�����f�[�^�̃T���v����

#define MAXDATASIZE 1048576						// �K���ɑ傫�Ȑ�
												// 65536 x 16
												// 100Hz�T���v�����O�Ŗ�U���Ԏ㕪

#define DATASORT 7			// �f�[�^�̎�ނ͂V���
							// SEQ, Ax, Ay, Az, Gx, Gy, Gz, Temperature

#define PACKETSIZE 15		// ���C�����X�ʐM�ɂ�����P�p�P�b�g������̃f�[�^�o�C�g��
#define PREAMBLE 0x65		// �p�P�b�g�̐擪�������f�[�^�i�v���A���u���j

#define SAMPLING 100		// �T���v�����O���g�� [Hz]

int databuf[DATASORT][MAXDATASIZE];				// �{���͂���Ȃ��Ƃ����Ă͂����Ȃ�
int errcnt;										// �ʐM�G���[�̉񐔂𐔂���ϐ�

int xaxis;					// �O���t�`�掞�̎��ԕ��@�i�P�ʁF�b�j
int yaxis;					// �O���u�`����s�����̔ԍ��i�P�`�V�j

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

	// �񓯊��h�n���[�h�Ȃ̂Ń^�C���A�E�g�͖���
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
	// COM�|�[�g���I�[�o���b�v�h���[�h�i�񓯊��ʐM���[�h�j�ɂăI�[�v�����Ă���

	if( hCOMnd == INVALID_HANDLE_VALUE){
		return -1;
	}

	GetCommProperties( hCOMnd, &myComProp);
	GetCommState(hCOMnd, &myDCB);
//	if( myComProp.dwSettableBaud & BAUD_128000)
//		myDCB.BaudRate = CBR_128000;
//	else
	myDCB.BaudRate = CBR_115200;		// 115.2Kbps���[�h��WindowsAPI�͐������F�����Ȃ�
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

// �I�[�o�[���b�v�h���[�h�ł́A�^�C���A�E�g�l�͈Ӗ����Ȃ��Ȃ�

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

// �I�[�o�[���b�v�h���[�h�ŁA�����ʐM�I�Ȃ��Ƃ��s�����߂̃p�b�`�R�[�h
			ReadFile( hCOMnd, rbuf, 1, &length, &rovl);
			while(1){
				if( HasOverlappedIoCompleted(&rovl)) break;
			}
		}
	}

	return d;
}


// �R�[���o�b�N�֐��ɂ��V���A���ʐM�󋵂̒ʒm����

int rcomp;

VOID CALLBACK FileIOCompletionRoutine( DWORD err, DWORD n, LPOVERLAPPED ovl)
{
	rcomp = n;			// �ǂݍ��񂾃o�C�g�������̂܂܃O���[�o���ϐ��ɕԂ�
}

// �����p�P�b�g����M���邽�߂̃X���b�h

#define RINGBUFSIZE 1024		// ��M�p�����O�o�b�t�@�̃T�C�Y�i�o�C�g���j

unsigned __stdcall serialchk( VOID * dummy)
{
	DWORD myErrorMask;						// ClearCommError ���g�p���邽�߂̕ϐ�
	COMSTAT	 myComStat;						// ��M�o�b�t�@�̃f�[�^�o�C�g�����擾���邽�߂Ɏg�p����
	unsigned char buf[RINGBUFSIZE];			// �����p�P�b�g����M���邽�߂̈ꎞ�o�b�t�@

	unsigned char ringbuffer[RINGBUFSIZE];	// �p�P�b�g��͗p�����O�o�b�t�@
	int rpos, wpos, rlen;					// �����O�o�b�t�@�p�|�C���^�iread, write)�A�f�[�^�o�C�g��
	int rpos_tmp;							// �f�[�^��͗pread�ʒu�|�C���^
	int rest;								// ClearCommError�ɂē�����f�[�^�o�C�g�����L�^����

	int seq, expected_seq;					// ���[�V�����Z���T�����M����V�[�P���X�ԍ��i�W�r�b�g�j
											// �ƁA���̊��Ғl�i�G���[���Ȃ���Ύ�M����l�j

	unsigned char packet[PACKETSIZE];		// �p�P�b�g��͗p�o�b�t�@ �i�P�p�P�b�g���j
	int i, chksum, tmp;

	expected_seq = 0;						// �ŏ��Ɏ�M�����ׂ�SEQ�̒l�̓[��
	rpos = wpos = 0;						// �����O�o�b�t�@�̓ǂݏo���y�я������݃|�C���^��������
	rlen = 0;								// �����O�o�b�t�@�Ɏc���Ă���L���ȃf�[�^���i�o�C�g�j

	while(rf){
		rcomp = 0;					// FileIOCompletionRoutine���Ԃ���M�o�C�g�����N���A���Ă���
		// �܂��͖����p�P�b�g�̐擪�̂P�o�C�g��ǂݏo���ɍs��
		ReadFileEx( hCOMnd, buf, 1, &rovl, FileIOCompletionRoutine);

		while(1){
			SleepEx( 100, TRUE);	// �ő��100�~���b = 0.1�b�̊ԃX���[�v���邪�A���̊Ԃ�
			if(rcomp == 1) break;	// I/O�����R�[���o�b�N�֐������s�����ƃX���[�v����������

			if(!rf){				// �O���v���O��������X���b�h�̒�~���w�����ꂽ���̏���
				CancelIo(hCOMnd);	// ���s�ς݂�ReadFileEx������������Ă���
				break;
			}
									// �f�[�^�������Ă��Ȃ����ԑтł́A��M�X���b�h���̂��̕���
									// �����X�Ə�������Ă��邪�A�唼��SleepEx�̎��s�ɔ�₳���
									// ���ƂŁA�V�X�e���ɗ^���镉�ׂ��y�����Ă���B
		}

		if(!rf) break;				// �O���v���O��������X���b�h�̒�~���w�����ꂽ

		ringbuffer[wpos] = buf[0];	// �ŏ��̂P�o�C�g����M���ꂽ
		wpos++;						// �����O�o�b�t�@�̏������݈ʒu�|�C���^���X�V
		wpos = (wpos >= RINGBUFSIZE)?0:wpos;	// �P�����Ă�����|�C���^���[���ɖ߂��i�Ȃ̂�RING�j
		rlen++;						// �����O�o�b�t�@���̗L���ȃf�[�^�����{�P����

		ClearCommError( hCOMnd, &myErrorMask, &myComStat);	// ��M�o�b�t�@�̏󋵂𒲂ׂ�`�o�h

		rest = myComStat.cbInQue;	// ��M�o�b�t�@�ɓ����Ă���f�[�^�o�C�g����������

		if(rest == 0) continue;		// ���������Ă��Ȃ������̂Ŏ��̂P�o�C�g��҂��ɂ���

		rcomp = 0;
		ReadFileEx( hCOMnd, buf, rest, &rovl, FileIOCompletionRoutine);
									// ��M�o�b�t�@�ɓ����Ă���f�[�^��ǂݏo��
									// �����I�ɂ�rest�Ŏ�����鐔�̃f�[�^����M���邱�Ƃ��ł��邪�A
									// ����ɔ����ăf�[�^���s�����Ă��܂������̏������l����B

		SleepEx(16, TRUE);			// Windows�ɂ�����V���A���|�[�g�̕W�����C�e���V�i16msec�j
		if( rcomp != rest){
			CancelIo(hCOMnd);		// ClearCommError�Ŏ擾�����f�[�^�o�C�g���ɖ����Ȃ�
		}							// �f�[�^������M����Ȃ������̂ŁA��ɔ��s����ReadFileEx
									// ���L�����Z�����Ă���

		i = 0;
		while(rcomp > 0){			// rcomp�ɂ͓ǂݏo�����Ƃ̂ł����f�[�^�̃o�C�g���������Ă���
			ringbuffer[wpos] = buf[i];	// �����O�o�b�t�@�Ɏ�M�f�[�^��]������
			wpos++;
			wpos = (wpos >= RINGBUFSIZE)?0:wpos;
			rlen++;
			i++;
			rcomp--;
		}

		// ��������p�P�b�g��͂ɓ���

		while(1){					// �L���ȃp�P�b�g�ł�������͂��p������
			while( rlen > 0){
				if( ringbuffer[rpos] == PREAMBLE) break;
				rpos++;								// �擪��PREAMBLE�ł͂Ȃ�����
				rpos = (rpos >= RINGBUFSIZE)?0:rpos;
				rlen--;								// �L���ȃf�[�^�����P���炵�čēx�擪�𒲂ׂ�
			}

			if(rlen < PACKETSIZE) break;	// ��͂ɕK�v�ȃf�[�^�o�C�g���ɒB���Ă��Ȃ������̂�
											// �ŏ��̂P�o�C�g��҂����ɖ߂�

			rpos_tmp = rpos;	// �����O�o�b�t�@�����؂��邽�߂̉��|�C���^
								// �܂������O�o�b�t�@��ɂ���f�[�^���L���ł���ƕ��������킯�ł͂Ȃ�

			for( i = 0, chksum = 0; i < (PACKETSIZE-1); i++){
				packet[i] = ringbuffer[rpos_tmp];	// �Ƃ肠������͗p�o�b�t�@�Ƀf�[�^�𐮗񂳂���
				chksum += packet[i];
				rpos_tmp++;
				rpos_tmp = (rpos_tmp >= RINGBUFSIZE)?0:rpos_tmp;
			}

			if( (chksum & 0xff) != ringbuffer[rpos_tmp]){	// �`�F�b�N�T���G���[�Ȃ̂Ńp�P�b�g�͖���
				rpos++;										// �擪�̂P�o�C�g���������
				rpos = (rpos >= RINGBUFSIZE)?0:rpos;
				rlen--;
				continue;	// ����PREAMBLE��T���ɂ���
			}

			// PREAMBLE�A�`�F�b�N�T���̒l��������PACKETSIZ���̃f�[�^��packet[]�ɓ����Ă���

			seq = packet[1];
			databuf[0][datasize] = seq;

			if(seq != expected_seq){
				errcnt += (seq + 256 - expected_seq)%256;	// �p�P�b�g�G���[�����X�V����
			}
			expected_seq = (seq + 1)%256;					// ����seq�̊��Ғl��expected_seq�ɓ����

			for( i = 0; i < 6; i++){	// �����x�R���A�p���x�R���A�̂U�̃f�[�^��ǂݏo��
				tmp = (packet[i*2 + 2]<<8)&0xff00 | packet[i*2 + 3];	// ���̎��_�ł�unsigned
				databuf[i+1][datasize] = (tmp > 32767)?(tmp - 65536):tmp;
			}

			datasize++;
			datasize = (datasize >= MAXDATASIZE)?(MAXDATASIZE-1):datasize;

			if( datasize%10 == 0){								// 10�T���v����1��̊����Ń��b�Z�[�W�𔭐�������
				PostMessage( hDlg, WM_RCV, (WPARAM)1, NULL);	// 100Hz�T���v�����O�ł����0.1�b��1���ʂ��X�V�����
			}													// PostMessage()���̂ł͏������Ԃ͋ɂ߂ĒZ��

			rpos = (rpos + PACKETSIZE)%RINGBUFSIZE;			// �������ǂݏo�����f�[�^�������O�o�b�t�@���珜������
			rlen -= PACKETSIZE;								// �o�b�t�@�̎c��e�ʂ��X�V����
		}
	}
	_endthreadex(0);	// �X���b�h�����ł�����
	return 0;
}

// �A�v���P�[�V�����̃o�[�W�������Ɏg���� CAboutDlg �_�C�A���O

class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

// �_�C�A���O �f�[�^
	enum { IDD = IDD_ABOUTBOX };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV �T�|�[�g

// ����
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


// CPQ2016_GRAPHDlg �_�C�A���O

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


// CPQ2016_GRAPHDlg ���b�Z�[�W �n���h��

BOOL CPQ2016_GRAPHDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// "�o�[�W�������..." ���j���[���V�X�e�� ���j���[�ɒǉ����܂��B
	hDlg = this->m_hWnd;		// ���̃_�C�A���O�ւ̃n���h�����擾����

	// IDM_ABOUTBOX �́A�V�X�e�� �R�}���h�͈͓̔��ɂȂ���΂Ȃ�܂���B
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

	// ���̃_�C�A���O�̃A�C�R����ݒ肵�܂��B�A�v���P�[�V�����̃��C�� �E�B���h�E���_�C�A���O�łȂ��ꍇ�A
	//  Framework �́A���̐ݒ�������I�ɍs���܂��B
	SetIcon(m_hIcon, TRUE);			// �傫���A�C�R���̐ݒ�
	SetIcon(m_hIcon, FALSE);		// �������A�C�R���̐ݒ�

	// TODO: �������������ɒǉ����܂��B

	CButton* radio1 = (CButton*)GetDlgItem(IDC_RADIO1);
	radio1->SetCheck(1);	// Y-axis : acc X
	yaxis = 1;				// �N������acc X�̃{�^����������Ă����Ԃɂ���

	CButton* radio8 = (CButton*)GetDlgItem(IDC_RADIO8);
	radio8->SetCheck(1);	// X-axis : 2sec
	xaxis = 2;				// �N������2 sec�̃{�^����������Ă����Ԃɂ���
	
	rf = 0;			// �����p�P�b�g��M�X���b�h�͒�~
	datasize = 0;	// ���[�V�����f�[�^�̐����O�ɏ�����


	// �ݒ�t�@�C����ǂݍ��݁A�V���A���ʐM�p�|�[�g�ԍ��ƃt�@�C���ۑ���t�H���_��ݒ肷��
	CStdioFile pFile;
	CString buf;
	char pbuf[256], dirbuf[_MAX_PATH];
	char pathname[256];
	int i, comport, dirlen;

	for( i = 0; i < 256; i++){
		pbuf[i] = pathname[i] = 0x00;	// ���S�̂���NULL�Ŗ��߂Ă���
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

		pbuf[1] = pbuf[2];	// Unicode�ɑΉ����邽�߂̃p�b�`�R�[�h
		pbuf[2] = 0x00;

		comport = atoi(pbuf);	// �|�[�g�ԍ����e�L�X�g�t�@�C������擾���Ă���

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

	return TRUE;  // �t�H�[�J�X���R���g���[���ɐݒ肵���ꍇ�������ATRUE ��Ԃ��܂��B
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

// �_�C�A���O�ɍŏ����{�^����ǉ�����ꍇ�A�A�C�R����`�悷�邽�߂�
//  ���̃R�[�h���K�v�ł��B�h�L�������g/�r���[ ���f�����g�� MFC �A�v���P�[�V�����̏ꍇ�A
//  ����́AFramework �ɂ���Ď����I�ɐݒ肳��܂��B

void CPQ2016_GRAPHDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // �`��̃f�o�C�X �R���e�L�X�g

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// �N���C�A���g�̎l�p�`�̈���̒���
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// �A�C�R���̕`��
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}

// ���[�U�[���ŏ��������E�B���h�E���h���b�O���Ă���Ƃ��ɕ\������J�[�\�����擾���邽�߂ɁA
//  �V�X�e�������̊֐����Ăяo���܂��B
HCURSOR CPQ2016_GRAPHDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

LRESULT CPQ2016_GRAPHDlg::OnMessageRCV( WPARAM wParam, LPARAM lParam)
{
	CString s;
	static int rcnt = 0;

	// ��M�����p�P�b�g���Ɩ����ʐM�G���[�̉񐔂�\������
	rcnt++;

	s.Format( _T("Received : %d Error = %d"), rcnt, errcnt);
	msgED.SetWindowTextW(s);


	// �O���t��`�悷��

	CWnd* myPICT = GetDlgItem(IDC_GRAPH);	// ��ʏ�̃s�N�`���R���g���[���ƑΉ��t����
	CClientDC myPICTDC(myPICT);				// �f�o�C�X�R���e�L�X�g���쐬
	CRect myPICTRECT;						// �`��̈��\���l�p�`�irectangle�j�̍��W

	int i, j, x, y;
	int w, h;
	int gsamples;

	gsamples = xaxis*SAMPLING;		// �`��Ɋ֗^����T���v���̐�

	myPICT->GetClientRect(myPICTRECT);
	w = myPICTRECT.Width();
	h = myPICTRECT.Height();

	CPen myPEN( PS_SOLID, 1, RGB( 0, 0, 0));	// �y�������F�ɐݒ�
	CPen* oldPEN = myPICTDC.SelectObject(&myPEN);

	myPICTDC.FillSolidRect( myPICTRECT, RGB( 255, 255, 255));

	if( datasize < gsamples){	// ��M�f�[�^�̃T���v��������ʕ�����菭�Ȃ��ꍇ
		y = databuf[yaxis][0];
		y = ((y + 32768)*h)>>16;	// �c���͈̔͂� -32768 �` +32768�ɐݒ肵�Ă���
		myPICTDC.MoveTo( 0, y);		// �y�����ŏ��̃f�[�^�������ʒu�ֈړ��iMove�j������

		for( i = 1; i < datasize; i++){
			x = i*w/gsamples;
			y = databuf[yaxis][i];
			y = ((y + 32768)*h)>>16;	// 16�r�b�g�E�V�t�g���邱�Ƃ�655536�Ŋ����Ă���
			myPICTDC.LineTo( x, y);		// �y�����f�[�^�������ʒu�ւƐ��������Ȃ���ړ�������
		}
	}else{						// ��ʕ����̎�M�f�[�^�����ɂ���ꍇ
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

	myPICTDC.SelectObject(oldPEN);	// �g�����y�������̏�Ԃɖ߂��Ă���

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
			// SEQ, Ax, Ay, Az, Gx, Gy, Gz ���x lx ly lz e4x e4y e4z alpha�@in integer
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
